#!/usr/bin/env bash
# macos_harnesses.sh (MAC-001 Task 10) runs every harness of a folder and
# holds the skips to its list. Tested here on fixture folders, on any host:
# it passes when each harness passes and every line that speaks of skipping
# is on its list, and fails, naming the harness, on a failing one, on a skip
# line of any spelling that is not on the list, on a harness that prints
# nothing, and on a folder with no harness.
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
expect_contains "the unlisted skip is named" "test_one printed a skip line that is not on the list" "$result"

# Arrange: a harness that passes after skipping part of itself, in another spelling.
write_harness "$work/partial" test_one 'echo "skip 3: no 32-bit build here"; echo "test_one: PASS (64-bit)"'
# Act
result=$(run_on "$work/partial")
# Assert
expect "a partial skip that is not on the list fails the run" "status=1" "${result%%$'\n'*}"
expect_contains "the partial skip's line is named" "test_one printed a skip line that is not on the list: skip 3: no 32-bit build here" "$result"

# Arrange: other wordings of the same thing.
write_harness "$work/wording" test_one 'echo "test_one: skipped the other half"; echo "test_one: PASS"'
write_harness "$work/wording" test_two 'echo "probe: the 32-bit half did not run"; echo "test_two: PASS"'
write_harness "$work/wording" test_three 'echo "OK (skipped=2)"'
# Act
result=$(run_on "$work/wording")
# Assert
expect_contains "skipped, did not run and a unittest skip count are all skip lines" \
    "macos_harnesses: 0 passed, 0 skipped by name, 3 failed" "$result"

# Arrange: a partial skip that is on the list, and checks whose names speak of skipping.
write_harness "$work/listed" test_lp64_address \
    'echo "test_lp64_address: -no-pie case SKIP (Apple'"'"'s linker cannot place an arm64 image below 4 GB)"; echo "test_lp64_address: PASS (64-bit)"'
write_harness "$work/listed" test_one 'echo "ok: a missing tool is not skipped off macOS"; echo "test_one: PASS"'
# Act
result=$(run_on "$work/listed")
# Assert
expect "a listed partial skip and a check named for skipping pass" "status=0" "${result%%$'\n'*}"
expect_contains "a partial skip still counts as a pass, and is shown" \
    "macos_harnesses: 2 passed, 0 skipped by name, 0 failed" "$result"
expect_contains "the listed partial skip is shown" "PART test_lp64_address: test_lp64_address: -no-pie case SKIP" "$result"

# Arrange: a harness that exits 0 having printed nothing ran nothing anyone can see.
write_harness "$work/silent" test_one 'true'
# Act
result=$(run_on "$work/silent")
# Assert
expect "a harness that prints nothing fails the run" "status=1" "${result%%$'\n'*}"
expect_contains "the silent harness is named" "FAIL test_one printed nothing" "$result"

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
