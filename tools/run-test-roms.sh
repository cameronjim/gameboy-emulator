#!/usr/bin/env bash
# integration gate: runs every manifest entry through the headless harness
set -uo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR=${BUILD_DIR:-build}
harness="$BUILD_DIR/gbrom_harness"
if [ ! -x "$harness" ]; then
    echo "harness not built at $harness" >&2
    exit 2
fi

trim() {
    echo "$1" | sed 's/^ *//; s/ *$//'
}

fail=0
total=0
passed=0
while IFS='|' read -r rom channel expect budget flags; do
    rom=$(trim "$rom")
    case "$rom" in '' | '#'*) continue ;; esac
    channel=$(trim "$channel")
    expect=$(trim "$expect")
    budget=$(trim "$budget")
    flags=$(trim "$flags")
    total=$((total + 1))
    if "$harness" "tests/roms/vendor/$rom" "$channel" "$expect" "$budget" > /dev/null 2>&1; then
        if [ "$flags" = "xfail" ]; then
            echo "XPASS $rom (unexpected pass, update manifest)"
        else
            echo "PASS  $rom"
        fi
        passed=$((passed + 1))
    else
        if [ "$flags" = "xfail" ]; then
            echo "XFAIL $rom (expected)"
        else
            echo "FAIL  $rom"
            fail=1
        fi
    fi
done < tests/roms/manifest.txt
echo "$passed/$total passed"
exit $fail
