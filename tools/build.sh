#!/usr/bin/env bash
# Builds hyprlang2lua. Run it inside the nix devShell (tools/nix-run.sh does that).
#
# Two compile groups, deliberately:
#   - this project's sources, built with every warning on and -Werror.
#   - the Hyprland sources it reuses, included with -isystem and built without -Werror,
#     because their warnings belong to Hyprland's build, not to this one.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

CXX="${CXX:-g++}"
LEGACY="third_party/hyprland-legacy/src"
OUT="${OUT:-build/hyprlang2lua}"

# STATIC=1 links everything into one file for release; the default dynamic build takes
# its libraries from the nix store, which is what the devShell wants.
PKGS="${PKGS:-hyprlang hyprutils hyprgraphics pixman-1 wayland-server xkbcommon}"
STATIC="${STATIC:-0}"

CFLAGS="$(pkg-config --cflags $PKGS)"
LDFLAGS="$(pkg-config --libs $PKGS)"

if [[ "$STATIC" == "1" ]]; then
    LDFLAGS="-static $LDFLAGS"
    # the vendored headers reach for wayland and xkbcommon declarations even though this
    # program talks to neither, so their include paths are still needed
    CFLAGS="$CFLAGS $(pkg-config --cflags wayland-server xkbcommon 2>/dev/null || true)"
    [[ -n "${HYPRGRAPHICS_INCLUDE:-}" ]] && CFLAGS="$CFLAGS -isystem $HYPRGRAPHICS_INCLUDE"
fi

WARNINGS=(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    -Werror
)

mkdir -p build/gen build/obj

# generated tables, derived from the pinned submodules
./tools/gen_special_values.sh "$LEGACY/config/legacy/ConfigManager.cpp" build/gen/SpecialValues.gen.hpp
./tools/gen_rule_names.sh third_party/hyprland/src build/gen/RuleNames.gen.hpp

# Hyprland sources this converter reuses rather than reimplements
REUSED=(
    "$LEGACY"/config/values/ConfigValues.cpp
    "$LEGACY"/config/values/types/*.cpp
    "$LEGACY"/config/shared/parserUtils/ParserUtils.cpp
    "$LEGACY"/helpers/env/Env.cpp
)

OWN=(src/*.cpp src/handlers/*.cpp)

objects=()

for f in "${REUSED[@]}"; do
    obj="build/obj/reused_$(echo "$f" | tr '/.' '__').o"
    # shellcheck disable=SC2086
    $CXX -std=c++26 -O2 -c "$f" -o "$obj" -Ibuild/gen -I"$LEGACY" $CFLAGS
    objects+=("$obj")
done

for f in "${OWN[@]}"; do
    obj="build/obj/own_$(basename "$f" .cpp).o"
    # shellcheck disable=SC2086
    $CXX -std=c++26 -O2 -c "$f" -o "$obj" "${WARNINGS[@]}" -Ibuild/gen -isystem "$LEGACY" $CFLAGS
    objects+=("$obj")
done

# shellcheck disable=SC2086
$CXX -std=c++26 -O2 -o "$OUT" "${objects[@]}" $LDFLAGS

# a static build runs in the nix sandbox, where regenerating fixtures would write into
# the source tree; the dev build keeps them current instead
if [[ "$STATIC" != "1" ]]; then
    ./tools/gen_fixtures.sh > /dev/null
fi

echo "built $OUT"
