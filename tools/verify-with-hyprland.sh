#!/usr/bin/env bash
# Checks converted configs against a real Hyprland, not against this project's idea of one.
#
# Hyprland has a mode for exactly this: `Hyprland --verify-config -c FILE` parses the
# config, reports any errors, and exits without starting a compositor, so it needs no
# seat, no DRM device and no Wayland session.
#
# Run this inside an Arch container that has the hyprland package installed, with the
# converter binary available (BIN, default build/hyprlang2lua).
#
# usage: tools/verify-with-hyprland.sh

set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

BIN="${BIN:-build/hyprlang2lua}"
HYPRLAND="${HYPRLAND:-Hyprland}"
status=0

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/hyprland-verify}"
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

echo "hyprland: $("$HYPRLAND" --version | head -2 | tr '\n' ' ')"
echo "pacman:   $(pacman -Q hyprland 2>/dev/null || echo 'not a pacman install')"
echo "converter: $BIN"
echo

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for conf in test/fixtures/*.conf; do
    name=$(basename "$conf" .conf)
    lua="$work/$name.lua"

    # convert with the binary under test rather than reusing the committed output, so this
    # checks the converter and not a file someone could have edited
    "$BIN" "$conf" > "$lua" 2>/dev/null

    if ! diff -q "$lua" "${conf%.conf}.lua" > /dev/null; then
        echo "  FAIL $name: the binary's output differs from the committed .lua"
        status=1
        continue
    fi

    out=$("$HYPRLAND" --verify-config -c "$lua" 2>&1)
    code=$?

    # --verify-config exits non-zero when the config has errors
    if [[ $code -ne 0 ]]; then
        echo "  FAIL $name.lua rejected by Hyprland (exit $code)"
        echo "$out" | grep -iE "error|invalid|unknown|expected" | head -20 | sed 's/^/        /'
        status=1
        continue
    fi

    # an accepted config can still carry complaints; treat them as failures too, since the
    # point is that the converter targets this version of Hyprland exactly
    if echo "$out" | grep -qiE "config error|invalid|unknown (option|field|keyword)"; then
        echo "  FAIL $name.lua parsed but Hyprland complained"
        echo "$out" | grep -iE "config error|invalid|unknown" | head -20 | sed 's/^/        /'
        status=1
        continue
    fi

    echo "  ok   $name.lua accepted by Hyprland $(("$(wc -l < "$lua")")) lines"
done

echo
if [[ $status -eq 0 ]]; then
    echo "every converted config is accepted by this Hyprland"
else
    echo "Hyprland rejected converted output"
fi

exit $status
