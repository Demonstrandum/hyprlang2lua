#!/usr/bin/env bash
# Regenerates the committed .lua next to every test/fixtures/*.conf.
#
# The generated files are committed so that a diff shows exactly what a commit changed
# about the converter's output. With --check, nothing is written and the script fails if
# any committed output is stale.
#
# usage: gen_fixtures.sh [--check]

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

BIN="${BIN:-build/hyprlang2lua}"
read -r -a BIN_CMD <<< "$BIN"
BIN_PATH="${BIN_CMD[-1]}"
check=false
[[ "${1:-}" == "--check" ]] && check=true

[[ -x "$BIN_PATH" ]] || { echo "gen_fixtures: $BIN_PATH not built" >&2; exit 1; }

status=0

for conf in test/fixtures/*.conf; do
    lua="${conf%.conf}.lua"
    # written through a file rather than a variable: command substitution eats trailing
    # newlines, which made the committed copy differ from the program's own output
    "${BIN_CMD[@]}" "$conf" > /tmp/gen_fixtures.out 2>/dev/null || true

    if $check; then
        if ! diff -u "$lua" /tmp/gen_fixtures.out > /dev/null 2>&1; then
            echo "gen_fixtures: $lua is stale" >&2
            diff -u "$lua" /tmp/gen_fixtures.out || true
            status=1
        fi
        continue
    fi

    cp /tmp/gen_fixtures.out "$lua"
    echo "wrote $lua"
done

exit $status
