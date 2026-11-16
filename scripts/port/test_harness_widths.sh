#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/midas-harness-widths.XXXXXX)
trap 'rm -rf "$work"' EXIT

# A stand-in compiler: FAKE_M32=reject refuses -m32 as aarch64 GCC does, and
# FAKE_POINTER_BYTES is what its preprocessor reports as __SIZEOF_POINTER__.
cat >"$work/gcc" <<'FAKE'
#!/usr/bin/env bash
for arg in "$@"; do
    if [[ $arg == -m32 && ${FAKE_M32:-accept} == reject ]]; then exit 1; fi
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
check_widths "a compiler without -m32 on a 64-bit host runs 64 only" "64 " FAKE_M32=reject FAKE_POINTER_BYTES=8
check_widths "a compiler with -m32 on a 64-bit host runs 32 then 64" "32 64 " FAKE_M32=accept FAKE_POINTER_BYTES=8
check_widths "a 32-bit host runs 32 only" "32 " FAKE_M32=accept FAKE_POINTER_BYTES=4
check_widths "MIDAS_HARNESS_WIDTHS=32 restricts to 32" "32 " FAKE_M32=accept FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=32
check_widths "MIDAS_HARNESS_WIDTHS=64 restricts to 64" "64 " FAKE_M32=accept FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=64
check_widths "a long macro list does not hide 64 under pipefail" "32 64 " FAKE_M32=accept FAKE_POINTER_BYTES=8 FAKE_NOISY=1
check_widths "a request for an unavailable width prints nothing" "" FAKE_M32=reject FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=32

# Arrange
status=0
# Act
(PATH="$work:$PATH" CC=gcc FAKE_M32=reject FAKE_POINTER_BYTES=4 bash -c 'source scripts/port/harness_widths.sh && harness_widths' >/dev/null 2>&1) || status=$?
# Assert
if [[ $status -ne 0 ]]; then echo "ok: no usable width fails"; else echo "FAIL: no usable width must fail"; failures=$((failures + 1)); fi

# Arrange
source scripts/port/harness_widths.sh
HARNESS_PENDING=("test_ads 64" "test_wardrobe 64")
# Act and Assert
if harness_pending test_ads 64; then echo "ok: test_ads 64 is pending"; else echo "FAIL: test_ads 64 is pending"; failures=$((failures + 1)); fi
if harness_pending test_dyn 64; then echo "FAIL: test_dyn 64 is not pending"; failures=$((failures + 1)); else echo "ok: test_dyn 64 is not pending"; fi
if harness_pending test_ads 32; then echo "FAIL: test_ads 32 is not pending"; failures=$((failures + 1)); else echo "ok: test_ads 32 is not pending"; fi

expect "harness_flags 32 prints -m32" "-m32" "$(harness_flags 32)"
expect "harness_flags 32 adds the harness's own flags" "-m32 -msse2 -mfpmath=sse" "$(harness_flags 32 -msse2 -mfpmath=sse)"
expect "harness_flags 64 prints nothing" "" "$(harness_flags 64 -msse2 -mfpmath=sse)"

if [[ $failures -ne 0 ]]; then
    echo "test_harness_widths: $failures failed"
    exit 1
fi
echo "test_harness_widths: PASS"
