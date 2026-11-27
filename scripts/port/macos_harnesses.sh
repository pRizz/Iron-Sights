#!/usr/bin/env bash
# Runs every test_*.sh harness on a Mac and holds the skips to a list
# (MAC-001, FORK-093): CI's macos-arm64-tests job runs it, and so does a
# developer checking a change on macOS.
#
#   bash scripts/port/macos_harnesses.sh [FOLDER]     # default: scripts/port
#
# Each harness runs with its output kept; a failing one is shown with its
# exit status, and the rest still run. A harness that cannot run on a Mac
# prints "NAME: SKIP (reason)" and exits 0; such a line passes only when it
# is on ALLOWED_SKIPS below, so a harness cannot start skipping its body
# unnoticed. Prints "macos_harnesses: P passed, S skipped by name, F failed"
# and exits 1 unless F is 0 and every skip was on the list. A folder with
# no harness fails: nothing ran.
set -euo pipefail
cd "$(dirname "$0")/../.."
folder=${1:-scripts/port}

# test_web_touch skips only where emcc is missing (GitHub's macOS runner, a
# Mac without Emscripten); test_xwayland_pointer always skips off Linux.
ALLOWED_SKIPS=(
    "test_web_touch: SKIP (emcc not installed)"
    "test_xwayland_pointer: SKIP (X11 code, Linux only)"
)

work=$(mktemp -d /tmp/midas-macos-harnesses-run.XXXXXX)
trap 'rm -rf "$work"' EXIT

# True when LINE is on ALLOWED_SKIPS.
skip_allowed() {
    local allowed
    for allowed in "${ALLOWED_SKIPS[@]}"; do
        if [[ $allowed == "$1" ]]; then
            return 0
        fi
    done
    return 1
}

shopt -s nullglob
harnesses=("$folder"/test_*.sh)
shopt -u nullglob
if [[ ${#harnesses[@]} -eq 0 ]]; then
    echo "macos_harnesses: no test_*.sh in $folder" >&2
    exit 1
fi

passed=0 skipped=0 failed=0
for harness in "${harnesses[@]}"; do
    name=$(basename "$harness" .sh)
    log=$work/$name.log
    status=0
    bash "$harness" >"$log" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "FAIL $name (exit $status)"
        cat "$log"
        failed=$((failed + 1))
        continue
    fi
    # A harness's own SKIP line: "NAME: SKIP (reason)".
    skip_line=$(grep -E "^$name: SKIP \(" "$log" || [[ $? -eq 1 ]])
    if [[ -z $skip_line ]]; then
        echo "PASS $name: $(tail -n 1 "$log")"
        passed=$((passed + 1))
    elif skip_allowed "$skip_line"; then
        echo "SKIP $skip_line"
        skipped=$((skipped + 1))
    else
        echo "FAIL $name printed a SKIP line that is not on the list: $skip_line"
        failed=$((failed + 1))
    fi
done

echo "macos_harnesses: $passed passed, $skipped skipped by name, $failed failed"
[[ $failed -eq 0 ]]
