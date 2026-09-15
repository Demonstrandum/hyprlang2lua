#!/usr/bin/env bash
# Builds the release binaries: one statically linked executable per Linux architecture.
#
# Each build runs in a nixos/nix container for that platform, so the x86_64 artefact is
# produced by an x86_64 toolchain rather than cross-compiled. Every binary is checked with
# tools/test.sh on its own architecture before it is kept.
#
# usage: tools/release.sh [version]

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

VERSION="${1:-$(cat VERSION)}"
OUTDIR="dist"

mkdir -p "$OUTDIR"

build_for() {
    local platform="$1" arch="$2"
    local out="$OUTDIR/hyprlang2lua-$VERSION-$arch-linux"

    echo "==> building $arch ($platform)"

    docker run --rm --platform "$platform" \
        -v "$PWD:/work" -v "hyprlang2lua-nix-$arch:/nix" -w /work \
        -e NIX_CONFIG="experimental-features = nix-command flakes" \
        nixos/nix:latest \
        bash -c "
            set -e
            nix develop .#static --command bash -c '
                STATIC=1 PKGS=\"hyprlang hyprutils\" OUT=build/hyprlang2lua-$arch ./tools/build.sh
                strip build/hyprlang2lua-$arch
                BIN=build/hyprlang2lua-$arch ./tools/test.sh
            '
        "

    cp "build/hyprlang2lua-$arch" "$out"
    echo "==> $out"
    file "$out"
    sha256sum "$out" 2>/dev/null || shasum -a 256 "$out"
}

build_for linux/arm64 aarch64
build_for linux/amd64 x86_64

echo
echo "release $VERSION:"
ls -la "$OUTDIR"
