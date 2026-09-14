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

PKGS="hyprlang hyprutils hyprgraphics pixman-1 wayland-server xkbcommon"
CFLAGS="$(pkg-config --cflags $PKGS)"
LDFLAGS="$(pkg-config --libs $PKGS)"

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
    "$LEGACY"/helpers/Color.cpp
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

./tools/gen_fixtures.sh > /dev/null

echo "built $OUT"
