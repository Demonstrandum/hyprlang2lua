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
check=false
[[ "${1:-}" == "--check" ]] && check=true

[[ -x "$BIN" ]] || { echo "gen_fixtures: $BIN not built" >&2; exit 1; }

status=0

for conf in test/fixtures/*.conf; do
    lua="${conf%.conf}.lua"
    actual="$("$BIN" "$conf" 2>/dev/null || true)"

    if $check; then
        if ! diff -u "$lua" <(printf '%s' "$actual") > /dev/null 2>&1; then
            echo "gen_fixtures: $lua is stale" >&2
            diff -u "$lua" <(printf '%s' "$actual") || true
            status=1
        fi
        continue
    fi

    printf '%s' "$actual" > "$lua"
    echo "wrote $lua"
done

exit $status
