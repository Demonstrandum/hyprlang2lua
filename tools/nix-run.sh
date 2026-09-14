#!/usr/bin/env bash
# Runs a command inside the project's nix devShell, in a Linux container.
#
# The dependencies (hyprlang, hyprutils, hyprgraphics) are Linux-only in nixpkgs, so the
# nix environment lives in a container even when the checkout is on macOS. The nix store
# is kept in a named volume so repeat runs do not re-download it.
#
# usage: tools/nix-run.sh <command...>

set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exec docker run --rm -it \
    -v "$repo:/work" \
    -v hyprlang2lua-nix:/nix \
    -w /work \
    -e NIX_CONFIG="experimental-features = nix-command flakes" \
    nixos/nix:latest \
    nix develop --command bash -c "$*"
