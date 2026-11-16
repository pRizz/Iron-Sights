#!/usr/bin/env bash
# GE_POINTER_BITS selects the 32-bit build (default) or the 64-bit one. Only
# configuring is tested (compile and link commands are read from the generated
# build files), plus small canary sources compiled with a recorded 64-bit C
# command; CI's lp64_diagnostics.py gate compiles the game itself.
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

# The game's link command, as the Makefile generator writes it at configure time.
link_file() { echo "$work/$1/CMakeFiles/ge.dir/link.txt"; }
# Called outside any condition, so its failure stops the script.
require_link() { [[ -f $(link_file "$1") ]] || fail "$1: no link command at $(link_file "$1")"; }
# Whole words only: -no-pie must not match inside another option.
has_link_flag() { grep -qE -- "(^| )$2( |\$)" "$(link_file "$1")"; }

# 1. The default is today's 32-bit build.
expect_configure default
has_flag default '-m32' || fail "default build lacks -m32"
echo "ok 1: default configure is 32-bit"
require_link default
for flag in -m32 -no-pie; do
    has_link_flag default "$flag" || fail "default link lacks $flag"
done
echo "ok 1b: default link has -m32 and -no-pie"

# 2. 64 drops every 32-bit-only flag and pins float contraction off.
expect_configure bits64 -DGE_POINTER_BITS=64
for flag in -m32 -msse2 -mfpmath=sse; do
    if has_flag bits64 "$flag"; then
        fail "64-bit build still uses $flag"
    fi
done
has_flag bits64 '-ffp-contract=off' || fail "64-bit build lacks -ffp-contract=off"
echo "ok 2: 64-bit configure has no 32-bit flags and has -ffp-contract=off"
require_link bits64
for flag in -m32 -no-pie; do
    if has_link_flag bits64 "$flag"; then
        fail "64-bit link still uses $flag"
    fi
done
echo "ok 2b: 64-bit link has neither -m32 nor -no-pie"

# 2c. At 64 bits the three int/pointer conversions are compile errors: a C
# entry's own recorded command rejects one canary source per conversion, and
# the message names the -Werror= flag (so -w or -Wno-error cannot hide it).
c_command() {
    python3 - "$work/$1/compile_commands.json" <<'PY'
import json, shlex, sys
for entry in json.load(open(sys.argv[1])):
    if entry["file"].endswith(".c"):
        argv = entry["arguments"] if "arguments" in entry else shlex.split(entry["command"])
        kept, skip = [], False
        for arg in argv:
            if skip:
                skip = False
            elif arg in ("-o", "-MT", "-MF"):
                skip = True
            elif arg not in ("-c", "-MD", "-MMD", entry["file"]):
                kept.append(arg)
        print(entry["directory"])
        print(shlex.join(kept + ["-fsyntax-only"]))
        break
else:
    sys.exit("no C entry in compile_commands.json")
PY
}
mapfile -t canary_cmd < <(c_command bits64)
[[ ${#canary_cmd[@]} -eq 2 ]] || fail "bits64: no C compile command recorded"
check_canary() {
    local flag=$1 source=$2 status=0
    printf '%s\n' "$source" >"$work/canary-$flag.c"
    (cd "${canary_cmd[0]}" && eval "${canary_cmd[1]}" '"$work/canary-$flag.c"') \
        >"$work/canary-$flag.log" 2>&1 || status=$?
    if [[ $status -eq 0 ]]; then
        fail "64-bit C flags accept the $flag canary: $source"
    fi
    grep -qF -- "-Werror=$flag" "$work/canary-$flag.log" || {
        cat "$work/canary-$flag.log" >&2
        fail "64-bit $flag canary did not fail on -Werror=$flag"
    }
}
check_canary int-conversion 'int f(char *p) { return p; }'
check_canary pointer-to-int-cast 'int g(char *p) { return (int)p; }'
check_canary int-to-pointer-cast 'char *h(int i) { return (char *)i; }'
echo "ok 2c: 64-bit C flags reject each int/pointer conversion"

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
