#!/usr/bin/env bash
# macos_harnesses.sh (MAC-001 Task 10) runs every harness of a folder and
# holds the skips to its list. Tested here on fixture folders, on any host:
# it passes when each harness passes or prints an allowed SKIP line, and
# fails, naming the harness, on a failing one, on a SKIP line that is not on
# the list, and on a folder with no harness.
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/midas-macos-harnesses.XXXXXX)
trap 'rm -rf "$work"' EXIT

failures=0
expect() {
    local name=$1 expected=$2 actual=$3
    if [[ $actual == "$expected" ]]; then
        echo "ok: $name"
    else
        echo "FAIL: $name: expected [$expected], got [$actual]"
        failures=$((failures + 1))
    fi
}
expect_contains() {
    local name=$1 wanted=$2 actual=$3
    if [[ $actual == *"$wanted"* ]]; then
        echo "ok: $name"
    else
        echo "FAIL: $name: no [$wanted] in [$actual]"
        failures=$((failures + 1))
    fi
}

# Writes FOLDER/NAME.sh holding BODY.
write_harness() {
    local folder=$1 name=$2 body=$3
    mkdir -p "$folder"
    printf '#!/usr/bin/env bash\nset -euo pipefail\n%s\n' "$body" >"$folder/$name.sh"
}
# Runs the runner on FOLDER; prints "status=N" and then its output.
run_on() {
    local status=0 output
    output=$(bash scripts/port/macos_harnesses.sh "$1" 2>&1) || status=$?
    echo "status=$status"
    echo "$output"
}

# Arrange
write_harness "$work/pass" test_one 'echo "test_one: PASS (64-bit)"'
write_harness "$work/pass" test_xwayland_pointer 'echo "test_xwayland_pointer: SKIP (X11 code, Linux only)"'
write_harness "$work/pass" not_a_harness 'exit 1'
# Act
result=$(run_on "$work/pass")
# Assert
expect "passing harnesses and an allowed skip pass" "status=0" "${result%%$'\n'*}"
expect_contains "the summary counts passes and skips" "macos_harnesses: 1 passed, 1 skipped by name, 0 failed" "$result"

# Arrange
write_harness "$work/fail" test_one 'echo "test_one: PASS (64-bit)"'
write_harness "$work/fail" test_two 'echo "boom" >&2; exit 3'
write_harness "$work/fail" test_three 'echo "test_three: PASS (64-bit)"'
# Act
result=$(run_on "$work/fail")
# Assert
expect "a failing harness fails the run" "status=1" "${result%%$'\n'*}"
expect_contains "the failing harness is named with its status" "FAIL test_two (exit 3)" "$result"
expect_contains "the failing harness's output is shown" "boom" "$result"
expect_contains "the harnesses after it still run" "macos_harnesses: 2 passed, 0 skipped by name, 1 failed" "$result"

# Arrange: a harness that skips its body without being on the list.
write_harness "$work/skip" test_one 'echo "test_one: SKIP (tool not installed)"'
# Act
result=$(run_on "$work/skip")
# Assert
expect "a skip that is not on the list fails the run" "status=1" "${result%%$'\n'*}"
expect_contains "the unlisted skip is named" "test_one printed a SKIP line that is not on the list" "$result"

# Arrange
mkdir -p "$work/empty"
# Act
result=$(run_on "$work/empty")
# Assert
expect "a folder with no harness fails instead of passing on nothing" "status=1" "${result%%$'\n'*}"

if [[ $failures -ne 0 ]]; then
    echo "test_macos_harnesses: $failures failed"
    exit 1
fi
echo "test_macos_harnesses: PASS"
