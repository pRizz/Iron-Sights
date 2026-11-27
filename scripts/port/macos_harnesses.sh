#!/usr/bin/env bash
# Runs every test_*.sh harness on a Mac and holds the skips to a list
# (MAC-001, FORK-093): CI's macos-arm64-tests job runs it, and so does a
# developer checking a change on macOS.
#
#   bash scripts/port/macos_harnesses.sh [FOLDER]     # default: scripts/port
#
# Each harness runs with its output kept; a failing one is shown with its
# exit status, and the rest still run. A harness that cannot run on a Mac
# prints "NAME: SKIP (reason)" and exits 0, and others pass after skipping a
# part (a 32-bit half, a case the linker cannot build). Every line that
# speaks of skipping, in any spelling ("skip", "skipped", "SKIP", "did not
# run"; a line starting "ok: " is a passing check's own name and is not
# one), must start with an entry of ALLOWED_SKIPS below for that harness, so
# a harness cannot start skipping its body, or more of it, unnoticed. A
# harness that prints nothing fails: nothing shows that it ran. Prints
# "macos_harnesses: P passed, S skipped by name, F failed", S the harnesses
# that skipped whole, and exits 1 unless F is 0. A folder with no harness
# fails: nothing ran.
set -euo pipefail
cd "$(dirname "$0")/../.."
folder=${1:-scripts/port}

# "NAME|START OF LINE". The first two skip whole: test_web_touch only where
# emcc is missing (GitHub's macOS runner, a Mac without Emscripten), and
# test_xwayland_pointer always off Linux. The rest are parts: a Mac has no
# 32-bit build and no MinGW, and Apple's linker cannot place an image below
# 4 GB. test_enhanced_fonts' line is the game's log of the case under test,
# and test_save_transfer's two are tests of Windows file behaviour.
ALLOWED_SKIPS=(
    "test_web_touch|test_web_touch: SKIP (emcc not installed)"
    "test_xwayland_pointer|test_xwayland_pointer: SKIP (X11 code, Linux only)"
    "test_abi_probe|ABI probe: the 32-bit half did not run: gcc rejects -m32 on this host."
    "test_abi_probe|ABI probe: the Windows x64 half did not run: x86_64-w64-mingw32-gcc is not installed."
    "test_enhanced_fonts|ge: enhanced fonts: GOLDENEYE#CDE33EA7#4#1_rgb.png does not match its pinned checksum; skipped"
    "test_lp64_address|test_lp64_address: -no-pie case SKIP (Apple's linker cannot place an arm64 image below 4 GB)"
    "test_pointer_bits|skip 1, 1b: Darwin has no 32-bit build"
    "test_pointer_bits|skip 5a: i686-w64-mingw32-gcc is not installed"
    "test_pointer_bits|skip 5b, 5c: x86_64-w64-mingw32-gcc is not installed"
    "test_pointer_bits|skip 6: Darwin has no 32-bit build"
    "test_pointer_bits|ok 6b: 64-bit builds compile port_state_unavailable.c, not port_state.c, and skip the layout step"
    "test_pointer_bits|skip 7 (outside a headless build), 7b: Darwin has no 32-bit build"
    "test_pointer_bits|skip 7d: Darwin has no 32-bit oracle build"
    "test_pointer_bits|ok 7e: converter sources make incompatible pointer types errors at 64 bits (skip: Darwin has no 32-bit oracle build)"
    "test_pointer_bits|skip 8d: Darwin has no 32-bit video build"
    "test_save_transfer|OK (skipped=2)"
    "test_save_widths|test_save_widths: skipped the other width's half, the file comparison and the cross-width reads: gcc offers only the 64-bit build"
)

work=$(mktemp -d /tmp/midas-macos-harnesses-run.XXXXXX)
trap 'rm -rf "$work"' EXIT

# True when LINE of harness NAME starts with one of its ALLOWED_SKIPS entries.
skip_allowed() {
    local name=$1 line=$2 allowed
    for allowed in "${ALLOWED_SKIPS[@]}"; do
        if [[ ${allowed%%|*} == "$name" && $line == "${allowed#*|}"* ]]; then
            return 0
        fi
    done
    return 1
}

# True when LINE speaks of skipping.
is_skip_line() {
    local lower=${1,,}
    if [[ $1 == "ok: "* ]]; then
        return 1
    fi
    [[ $lower =~ (^|[^a-z])skip(s|ped)?([^a-z]|$) || $lower == *"did not run"* ]]
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
    if [[ ! -s $log ]]; then
        echo "FAIL $name printed nothing"
        failed=$((failed + 1))
        continue
    fi
    whole=0 unlisted=0
    parts=()
    while IFS= read -r line; do
        is_skip_line "$line" || continue
        if ! skip_allowed "$name" "$line"; then
            echo "FAIL $name printed a skip line that is not on the list: $line"
            unlisted=1
        elif [[ $line == "$name: SKIP ("* ]]; then
            whole=1
            echo "SKIP $line"
        else
            parts+=("$line")
        fi
    done <"$log"
    if [[ $unlisted -eq 1 ]]; then
        failed=$((failed + 1))
    elif [[ $whole -eq 1 ]]; then
        skipped=$((skipped + 1))
    else
        echo "PASS $name: $(tail -n 1 "$log")"
        for line in ${parts[@]+"${parts[@]}"}; do
            echo "PART $name: $line"
        done
        passed=$((passed + 1))
    fi
done

echo "macos_harnesses: $passed passed, $skipped skipped by name, $failed failed"
[[ $failed -eq 0 ]]
