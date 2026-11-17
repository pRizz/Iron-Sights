#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/midas-harness-widths.XXXXXX)
trap 'rm -rf "$work"' EXIT

# A stand-in compiler: FAKE_M32=reject refuses -m32, FAKE_MACHINE is what
# -dumpmachine prints (x86_64-linux-gnu unless set), FAKE_POINTER_BYTES is what
# its preprocessor reports as __SIZEOF_POINTER__, FAKE_DM_FAIL=1 makes the
# macro probe fail with a message and FAKE_DUMPMACHINE_FAIL=1 makes
# -dumpmachine fail with one.
cat >"$work/gcc" <<'FAKE'
#!/usr/bin/env bash
for arg in "$@"; do
    if [[ $arg == -m32 && ${FAKE_M32:-accept} == reject ]]; then exit 1; fi
    if [[ $arg == -dumpmachine && -n ${FAKE_DUMPMACHINE_FAIL:-} ]]; then echo "fake: no machine" >&2; exit 1; fi
    if [[ $arg == -dumpmachine ]]; then echo "${FAKE_MACHINE:-x86_64-linux-gnu}"; exit 0; fi
    if [[ $arg == -dM && -n ${FAKE_DM_FAIL:-} ]]; then echo "fake: cannot run cc1" >&2; exit 1; fi
    if [[ $arg == -dM ]]; then
        echo "#define __SIZEOF_POINTER__ ${FAKE_POINTER_BYTES:-8}"
        # A real compiler prints hundreds of macros after this one.
        if [[ -n ${FAKE_NOISY:-} ]]; then yes '#define FILLER 1' | head -n 20000 || exit $?; fi
    fi
done
exit 0
FAKE
chmod +x "$work/gcc"

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

check_widths() {
    local name=$1 expected=$2
    shift 2
    local actual
    actual=$(env PATH="$work:$PATH" CC=gcc "$@" \
        bash -c 'set -euo pipefail; source scripts/port/harness_widths.sh && harness_widths' 2>/dev/null | tr '\n' ' ' || true)
    expect "$name" "$expected" "$actual"
}

# Arrange / Act / Assert for each case: the environment, the call, the printed widths.
check_widths "an aarch64 compiler without -m32 runs 64 only" "64 " FAKE_M32=reject FAKE_MACHINE=aarch64-linux-gnu FAKE_POINTER_BYTES=8
check_widths "a compiler with -m32 on a 64-bit host runs 32 then 64" "32 64 " FAKE_M32=accept FAKE_POINTER_BYTES=8
check_widths "a 32-bit host runs 32 only" "32 " FAKE_M32=accept FAKE_POINTER_BYTES=4
check_widths "MIDAS_HARNESS_WIDTHS=32 restricts to 32" "32 " FAKE_M32=accept FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=32
check_widths "MIDAS_HARNESS_WIDTHS=64 restricts to 64" "64 " FAKE_M32=accept FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=64
check_widths "a long macro list does not hide 64 under pipefail" "32 64 " FAKE_M32=accept FAKE_POINTER_BYTES=8 FAKE_NOISY=1
check_widths "a request for an unavailable width prints nothing" "" FAKE_M32=reject FAKE_MACHINE=aarch64-linux-gnu FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=32
check_widths "an x86_64 compiler without -m32 fails and prints no width" "" FAKE_M32=reject FAKE_POINTER_BYTES=8
check_widths "an x86_64 compiler without -m32 runs 64 when restricted to it" "64 " FAKE_M32=reject FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=64

# Runs harness_widths with a stand-in compiler and prints its exit status and stderr on one line.
widths_failure() {
    local status=0 err
    err=$(env PATH="$work:$PATH" CC=gcc "$@" \
        bash -c 'set -euo pipefail; source scripts/port/harness_widths.sh && harness_widths' 2>&1 >/dev/null) || status=$?
    echo "status=$status $err"
}
expect_failure() {
    local name=$1 pattern=$2
    shift 2
    local actual
    actual=$(widths_failure "$@")
    if [[ $actual == status=[1-9]*"$pattern"* ]]; then
        echo "ok: $name"
    else
        echo "FAIL: $name: expected a failure mentioning [$pattern], got [$actual]"
        failures=$((failures + 1))
    fi
}

# Arrange / Act / Assert: the environment, the call, the failure and what it says.
expect_failure "no usable width fails" "cannot build any" FAKE_M32=reject FAKE_MACHINE=aarch64-linux-gnu FAKE_POINTER_BYTES=4
expect_failure "an x86_64 compiler without -m32 names the missing 32-bit toolchain" "32-bit toolchain is missing" FAKE_M32=reject FAKE_POINTER_BYTES=8
expect_failure "an i686 compiler without -m32 names the missing 32-bit toolchain" "32-bit toolchain is missing" FAKE_M32=reject FAKE_MACHINE=i686-linux-gnu FAKE_POINTER_BYTES=4
expect_failure "the missing toolchain message says how to restrict the run" "MIDAS_HARNESS_WIDTHS=64" FAKE_M32=reject FAKE_POINTER_BYTES=8
expect_failure "an unknown width is refused" "unknown width '16'" FAKE_M32=accept FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS="16"
expect_failure "a failing macro probe fails and shows the compiler's message" "fake: cannot run cc1" FAKE_M32=accept FAKE_POINTER_BYTES=8 FAKE_DM_FAIL=1
expect_failure "a failing -dumpmachine fails and shows the compiler's message" "fake: no machine" FAKE_M32=reject FAKE_POINTER_BYTES=8 FAKE_DUMPMACHINE_FAIL=1

# Arrange
source scripts/port/harness_widths.sh
# Act and Assert: at the close of LP64-003 no harness half is pending, so every
# harness runs at both widths (the plan's Review Focus 6).
expect "the shipped HARNESS_PENDING is empty" "" "${HARNESS_PENDING[*]}"
# Arrange
HARNESS_PENDING=("test_wardrobe 64" "x 32")
# Act and Assert
if harness_pending test_wardrobe 64; then echo "ok: test_wardrobe 64 is pending"; else echo "FAIL: test_wardrobe 64 is pending"; failures=$((failures + 1)); fi
if harness_pending test_dyn 64; then echo "FAIL: test_dyn 64 is not pending"; failures=$((failures + 1)); else echo "ok: test_dyn 64 is not pending"; fi
if harness_pending test_wardrobe 32; then echo "FAIL: test_wardrobe 32 is not pending"; failures=$((failures + 1)); else echo "ok: test_wardrobe 32 is not pending"; fi

expect "harness_flags 32 prints -m32" "-m32" "$(harness_flags 32)"
expect "harness_flags 32 adds the harness's own flags" "-m32 -msse2 -mfpmath=sse" "$(harness_flags 32 -msse2 -mfpmath=sse)"
expect "harness_flags 64 prints nothing" "" "$(harness_flags 64 -msse2 -mfpmath=sse)"

# A loop over "$(harness_widths)" runs zero times when the helper fails, and the
# harness passes having run nothing: a harness takes widths=$(harness_widths) on
# its own line so set -e stops it. Arrange / Act: the harnesses that loop directly.
direct_loops=$(grep -lE 'in[[:space:]]+"?\$\(harness_widths\)' scripts/port/*.sh | tr '\n' ' ' || true)
# Assert
expect "no harness loops directly over \$(harness_widths)" "" "$direct_loops"

if [[ $failures -ne 0 ]]; then
    echo "test_harness_widths: $failures failed"
    exit 1
fi
echo "test_harness_widths: PASS"
