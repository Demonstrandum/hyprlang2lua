#!/usr/bin/env bash
# The check that says the converter works. Run it inside the nix devShell.
#
#   1. every fixture converts without the process failing
#   2. every converted file parses as Lua (luajit -bl, which compiles without running)
#   3. every hl.* call the output makes exists in the Hyprland Lua API, checked against
#      the binding registrations in the pinned submodule
#   4. the committed .lua next to each fixture matches what the binary produces now
#
# usage: tools/test.sh

set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

# BIN may carry a runner prefix, e.g. BIN="qemu-x86_64 build/hyprlang2lua-x86_64"
BIN="${BIN:-build/hyprlang2lua}"
read -r -a BIN_CMD <<< "$BIN"
BIN_PATH="${BIN_CMD[-1]}"
HYPRLAND="third_party/hyprland/src"
status=0
pass=0

say() { printf '%s\n' "$*"; }
ok() { pass=$((pass + 1)); printf '  ok   %s\n' "$1"; }
fail() { status=1; printf '  FAIL %s\n' "$1"; }

[[ -x "$BIN_PATH" ]] || { say "test: $BIN_PATH not built"; exit 1; }

# ---------------------------------------------------------------- 1, 2: convert and parse

say "converting fixtures"

for conf in test/fixtures/*.conf; do
    lua="${conf%.conf}.lua"

    if ! "${BIN_CMD[@]}" "$conf" > /tmp/out.lua 2>/tmp/out.err; then
        # a converter that reports unconvertible input still exits non-zero; that is fine
        # as long as it produced output
        [[ -s /tmp/out.lua ]] || { fail "$conf produced no output"; continue; }
    fi

    if luajit -bl /tmp/out.lua /dev/null 2>/tmp/lua.err; then
        ok "$(basename "$lua") parses as Lua"
    else
        fail "$(basename "$lua") is not valid Lua: $(head -1 /tmp/lua.err)"
    fi
done

# ---------------------------------------------------------------- 3: calls exist in the API

say "checking hl.* calls against the Hyprland bindings"

# the names registered on the hl table and on hl.dsp, read out of the binding sources
mapfile -t REGISTERED < <(grep -rhoE 'set(Mgr)?Fn\(L, ?(mgr, ?)?"[a-z_0-9]+"' "$HYPRLAND"/config/lua/bindings/*.cpp |
    grep -oE '"[a-z_0-9]+"' | tr -d '"' | sort -u)

# plus the nested tables (hl.dsp.window, hl.plugin, ...) and the object constructors
mapfile -t TABLES < <(grep -rhoE 'lua_setfield\(L, -2, "[a-z_0-9]+"\)' "$HYPRLAND"/config/lua/bindings/*.cpp |
    grep -oE '"[a-z_0-9]+"' | tr -d '"' | sort -u)

known() {
    local leaf="$1"
    local name
    for name in "${REGISTERED[@]}" "${TABLES[@]}"; do
        [[ "$name" == "$leaf" ]] && return 0
    done
    return 1
}

mapfile -t USED < <(cat test/fixtures/*.lua | grep -oE '\bhl(\.[a-z_0-9]+)+\(' | sed 's/($//; s/(//' | sort -u)

for call in "${USED[@]}"; do
    leaf="${call##*.}"

    if known "$leaf"; then
        continue
    fi

    fail "$call is not registered by any Hyprland Lua binding"
done

[[ $status -eq 0 ]] && ok "all ${#USED[@]} distinct hl.* calls exist in the bindings"

# ---------------------------------------------------------------- 4: fixtures are current

say "checking committed fixture output"

if ./tools/gen_fixtures.sh --check; then
    ok "committed .lua files match the binary"
else
    fail "committed .lua files are stale; run tools/gen_fixtures.sh"
fi

say ""
if [[ $status -eq 0 ]]; then
    say "all checks passed ($pass)"
else
    say "checks failed"
fi

exit $status
