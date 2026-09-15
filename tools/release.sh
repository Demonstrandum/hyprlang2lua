#!/usr/bin/env bash
# Builds the release binaries: one statically linked executable per Linux architecture.
#
# Everything runs in one nixos/nix container on the host's own architecture. The foreign
# architecture is cross-compiled (nix pkgsCross) rather than emulated, because building a
# musl dependency tree under qemu is both slow and prone to failing in ways the target
# never would. The cross-built binary is then run under qemu-user, so both artefacts are
# tested rather than assumed.
#
# usage: tools/release.sh [version]

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

VERSION="${1:-$(cat VERSION)}"
OUTDIR="dist"

# keep the container on the host's own architecture; everything foreign is cross-compiled
case "$(uname -m)" in
    arm64 | aarch64) HOST_PLATFORM="linux/arm64" ;;
    *) HOST_PLATFORM="linux/amd64" ;;
esac

mkdir -p "$OUTDIR"

run_in_nix() {
    docker run --rm --platform "$HOST_PLATFORM" \
        -v "$PWD:/work" -v hyprlang2lua-nix:/nix -w /work \
        -e NIX_CONFIG="experimental-features = nix-command flakes" \
        nixos/nix:latest bash -c "$1"
}

build_for() {
    local shell="$1" arch="$2" qemu="$3"
    local bin="build/hyprlang2lua-$arch"
    local out="$OUTDIR/hyprlang2lua-$VERSION-$arch-linux"

    echo "==> building $arch via devShells.$shell"

    run_in_nix "
        set -e
        nix develop .#$shell --command bash -c '
            STATIC=1 PKGS=\"hyprlang hyprutils\" OUT=$bin ./tools/build.sh
            \$STRIP $bin 2>/dev/null || strip $bin
            BIN=\"$qemu $bin\" ./tools/test.sh
        '
    "

    cp "$bin" "$out"
    echo "==> $out"
}

build_for static-aarch64 aarch64 "qemu-aarch64"
build_for static-x86_64 x86_64 "qemu-x86_64"

echo
echo "release $VERSION"
for f in "$OUTDIR"/hyprlang2lua-"$VERSION"-*; do
    file "$f"
    shasum -a 256 "$f" 2>/dev/null || sha256sum "$f"
done
