#!/usr/bin/env bash
# GE_POINTER_BITS selects the 32-bit build (default) or the 64-bit one. Only
# configuring is tested: a 64-bit compile is not expected to work until LP64-002.
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/midas-pointer-bits.XXXXXX)
trap 'rm -rf "$work"' EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }

# Configure into a fresh directory; the log lands in $work/<name>.log.
# Returns CMake's exit status without tripping set -e.
configure() {
    local name=$1; shift
    local status=0
    cmake -S . -B "$work/$name" -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@" \
        >"$work/$name.log" 2>&1 || status=$?
    return "$status"
}

expect_configure() {
    local name=$1; shift
    if ! configure "$name" "$@"; then
        cat "$work/$name.log" >&2
        fail "$name: configure should have succeeded"
    fi
}

expect_failure() {
    local name=$1 message=$2; shift 2
    if configure "$name" "$@"; then
        fail "$name: configure should have failed"
    fi
    # CMake wraps long messages at 80 columns, so compare with whitespace collapsed.
    local flat
    flat=$(tr -s ' \n' '  ' <"$work/$name.log")
    [[ $flat == *"$message"* ]] || {
        cat "$work/$name.log" >&2
        fail "$name: output lacks \"$message\""
    }
}

# grep reads the file itself: grep -q on a pipe makes the writer die of SIGPIPE under pipefail.
has_flag() { grep -qF -- "$2" "$work/$1/compile_commands.json"; }

# 1. The default is today's 32-bit build.
expect_configure default
has_flag default '-m32' || fail "default build lacks -m32"
echo "ok 1: default configure is 32-bit"

# 2. 64 drops every 32-bit-only flag and pins float contraction off.
expect_configure bits64 -DGE_POINTER_BITS=64
for flag in -m32 -msse2 -mfpmath=sse; do
    if has_flag bits64 "$flag"; then
        fail "64-bit build still uses $flag"
    fi
done
has_flag bits64 '-ffp-contract=off' || fail "64-bit build lacks -ffp-contract=off"
echo "ok 2: 64-bit configure has no 32-bit flags and has -ffp-contract=off"

# 3. Any other width is refused.
expect_failure bad48 "GE_POINTER_BITS must be 32 or 64" -DGE_POINTER_BITS=48
echo "ok 3: 48 is refused"

# 4. 64-bit needs runtime ROM loading.
expect_failure fallback "GE_POINTER_BITS=64 requires runtime ROM loading (GE_ROMDATA_FALLBACK=OFF)" \
    -DGE_POINTER_BITS=64 -DGE_ROMDATA_FALLBACK=ON
echo "ok 4: 64 with the ROM fallback is refused"

# 5. Windows x64 is outside the plan. Needs the MinGW compiler the toolchain file names.
if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
    expect_failure windows "Windows x64 is outside the 64-bit plan; see scripts/port/native_64bit_plan.md" \
        -DGE_POINTER_BITS=64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
    echo "ok 5: 64 for Windows is refused"
else
    echo "skip 5: i686-w64-mingw32-gcc is not installed"
fi
echo "Pointer-bits configure checks passed"
