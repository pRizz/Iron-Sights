#!/usr/bin/env bash
# GE_POINTER_BITS selects the 32-bit build (default) or the 64-bit one. Only
# configuring is tested (compile and link commands are read from the generated
# build files), plus small canary sources compiled with recorded 64-bit C and
# C++ commands; CI's lp64_diagnostics.py gate compiles the game itself.
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

# 2c. At 64 bits the int/pointer conversions are compile errors. One recorded
# command per language compiles one canary source per conversion it gates and
# must reject each with GCC's own bracketed "[-Werror=flag]" text (so -w or
# -Wno-error cannot hide it), and must accept a clean source that converts
# through uintptr_t (so the rejection is the conversion's, not a broken
# command's). C gates all three; C++ gates int-to-pointer-cast, the one it
# only warns about (the other two are C++ errors without any flag). Only the
# video tree compiles C++.
expect_configure bits64video -DGE_POINTER_BITS=64 -DGE_HEADLESS=OFF -DGE_VIDEO=ON
entry_command() {
    python3 - "$work/$1/compile_commands.json" "$2" <<'PY'
import json, shlex, sys
for entry in json.load(open(sys.argv[1])):
    if entry["file"].endswith(sys.argv[2]):
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
    sys.exit(f"no {sys.argv[2]} entry in {sys.argv[1]}")
PY
}
mapfile -t c_cmd < <(entry_command bits64 .c)
[[ ${#c_cmd[@]} -eq 2 ]] || fail "bits64: no C compile command recorded"
mapfile -t cxx_cmd < <(entry_command bits64video .cpp)
[[ ${#cxx_cmd[@]} -eq 2 ]] || fail "bits64video: no C++ compile command recorded"

# Compile $3 as language $1 (c or cxx) into canary-$2; returns the compiler's status.
compile_canary() {
    local lang=$1 name=$2 source=$3 status=0 ext=c
    local -n cmd=${lang}_cmd
    [[ $lang == cxx ]] && ext=cpp
    printf '%s\n' "$source" >"$work/canary-$name.$ext"
    (cd "${cmd[0]}" && eval "${cmd[1]}" '"$work/canary-$name.$ext"') \
        >"$work/canary-$name.log" 2>&1 || status=$?
    return "$status"
}
check_canary() {
    local lang=$1 flag=$2 source=$3
    if compile_canary "$lang" "$lang-$flag" "$source"; then
        fail "64-bit $lang flags accept the $flag canary: $source"
    fi
    grep -qF -- "[-Werror=$flag]" "$work/canary-$lang-$flag.log" || {
        cat "$work/canary-$lang-$flag.log" >&2
        fail "64-bit $lang $flag canary did not fail with [-Werror=$flag]"
    }
}
check_clean() {
    local lang=$1 source=$2
    compile_canary "$lang" "$lang-clean" "$source" || {
        cat "$work/canary-$lang-clean.log" >&2
        fail "64-bit $lang flags reject a clean source: $source"
    }
}
check_clean c '#include <stdint.h>
char *ok(char *p) { return (char *)(uintptr_t)p; }'
check_canary c int-conversion 'int f(char *p) { return p; }'
check_canary c pointer-to-int-cast 'int g(char *p) { return (int)p; }'
check_canary c int-to-pointer-cast 'char *h(int i) { return (char *)i; }'
echo "ok 2c: 64-bit C flags reject each int/pointer conversion and accept a clean source"
check_clean cxx '#include <cstdint>
char *ok(char *p) { return (char *)(uintptr_t)p; }'
check_canary cxx int-to-pointer-cast 'char *h(int i) { return (char *)i; }'
echo "ok 2d: 64-bit C++ flags reject an int-to-pointer cast and accept a clean source"

# 2e. gfx_run_dl's command-word check (GE_LP64_DL_CHECK) is opt-in: the default
# 64-bit video build, the kind a player build is, lacks it; a sanitizer build has it.
mapfile -t gfx_cmd < <(entry_command bits64video port/fast3d/gfx_pc.cpp)
[[ ${#gfx_cmd[@]} -eq 2 ]] || fail "bits64video: no gfx_pc.cpp compile command recorded"
if [[ ${gfx_cmd[1]} == *-DGE_LP64_DL_CHECK* ]]; then
    fail "default 64-bit video build compiles gfx_pc.cpp with GE_LP64_DL_CHECK"
fi
expect_configure bits64videosan -DGE_POINTER_BITS=64 -DGE_HEADLESS=OFF -DGE_VIDEO=ON -DGE_SANITIZE=ON
mapfile -t gfx_san_cmd < <(entry_command bits64videosan port/fast3d/gfx_pc.cpp)
[[ ${#gfx_san_cmd[@]} -eq 2 ]] || fail "bits64videosan: no gfx_pc.cpp compile command recorded"
[[ ${gfx_san_cmd[1]} == *-DGE_LP64_DL_CHECK* ]] || fail "64-bit sanitizer video build lacks GE_LP64_DL_CHECK"
echo "ok 2e: GE_LP64_DL_CHECK only in the sanitizer build, not the default 64-bit video build"

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

# 6. Suspend images record 32-bit addresses, so a 64-bit build compiles the
# unavailable stand-in instead of the serializer and skips the post-build
# layout step; the 32-bit build keeps both. Each source is matched as a whole
# path in the recorded "file" field.
compiles() { grep -qF -- "/port/src/$2\"" "$work/$1/compile_commands.json"; }
has_layout_step() { grep -qF -- "embed_state_layout.py" "$work/$1/CMakeFiles/ge.dir/build.make"; }
compiles default port_state.c || fail "32-bit build does not compile port_state.c"
if compiles default port_state_unavailable.c; then
    fail "32-bit build compiles port_state_unavailable.c"
fi
has_layout_step default || fail "32-bit build lacks the embed_state_layout.py step"
echo "ok 6: 32-bit build compiles port_state.c and runs the layout step"
for tree in bits64 bits64video; do
    compiles "$tree" port_state_unavailable.c || fail "$tree: 64-bit build does not compile port_state_unavailable.c"
    if compiles "$tree" port_state.c; then
        fail "$tree: 64-bit build still compiles port_state.c"
    fi
    if has_layout_step "$tree"; then
        fail "$tree: 64-bit build still runs embed_state_layout.py"
    fi
done
echo "ok 6b: 64-bit builds compile port_state_unavailable.c, not port_state.c, and skip the layout step"

# 7. The ROM data converters (port/src/romconv_*.c) and the oracle
# (port_lp64_oracle.c) never reach a shipped 32-bit build: converters compile
# at 64 bits and in the opt-in 32-bit headless oracle build only, the oracle
# in that build only, and each converter source turns incompatible pointer
# types into errors (the 32-bit oracle build leaves -w off them, since GCC's
# -w silences a -Werror= too).
expect_failure oracle64 "GE_LP64_ORACLE=ON is a 32-bit build (GE_POINTER_BITS=32)" \
    -DGE_POINTER_BITS=64 -DGE_LP64_ORACLE=ON
expect_failure oraclevideo "GE_LP64_ORACLE=ON needs a headless build (GE_HEADLESS=ON)" \
    -DGE_HEADLESS=OFF -DGE_LP64_ORACLE=ON
echo "ok 7: the oracle is refused at 64 bits and outside a headless build"
lists_romconv() { grep -qF -- "/port/src/romconv_" "$work/$1/compile_commands.json"; }
if lists_romconv default; then
    fail "32-bit build without GE_LP64_ORACLE compiles a romconv_ source"
fi
for source in port_lp64_oracle.c port_lp64.c; do
    if compiles default "$source"; then
        fail "32-bit build without GE_LP64_ORACLE compiles $source"
    fi
done
echo "ok 7b: the default 32-bit build lists no romconv_ source, no oracle and no port_lp64.c"
for tree in bits64 bits64video; do
    compiles "$tree" romconv_common.c || fail "$tree: 64-bit build does not compile romconv_common.c"
    if compiles "$tree" port_lp64_oracle.c; then
        fail "$tree: 64-bit build compiles port_lp64_oracle.c"
    fi
done
echo "ok 7c: 64-bit builds compile the converters and not the oracle"
expect_configure oracle -DGE_LP64_ORACLE=ON
for source in romconv_common.c port_lp64_oracle.c port_lp64.c; do
    compiles oracle "$source" || fail "oracle build does not compile $source"
done
has_flag oracle '-m32' || fail "oracle build lacks -m32"
echo "ok 7d: the 32-bit oracle build compiles the converters, the oracle and port_lp64.c"
# Whole words of one recorded command: -w must not match inside -Wall.
command_has() { [[ " $1 " == *" $2 "* ]]; }
mapfile -t conv64 < <(entry_command bits64 port/src/romconv_common.c)
command_has "${conv64[1]}" -Werror=incompatible-pointer-types ||
    fail "64-bit romconv_common.c lacks -Werror=incompatible-pointer-types"
mapfile -t conv32 < <(entry_command oracle port/src/romconv_common.c)
command_has "${conv32[1]}" -Werror=incompatible-pointer-types ||
    fail "oracle romconv_common.c lacks -Werror=incompatible-pointer-types"
command_has "${conv32[1]}" -DGE_LP64_ORACLE || fail "oracle romconv_common.c lacks -DGE_LP64_ORACLE"
if command_has "${conv32[1]}" -w; then
    fail "oracle romconv_common.c is compiled with -w, which silences its -Werror="
fi
mapfile -t other32 < <(entry_command oracle port/src/port_lp64.c)
command_has "${other32[1]}" -w || fail "oracle port_lp64.c lost -w"
# sentry.cmake adds the crash adapter after add_executable; it needs -w too.
mapfile -t adapter32 < <(entry_command oracle port/crash/reporting_linux.c)
command_has "${adapter32[1]}" -w || fail "oracle port/crash/reporting_linux.c lacks -w"
echo "ok 7e: converter sources make incompatible pointer types errors at both widths; the oracle build keeps -w elsewhere, the crash adapter included"

# 8. The comparison build (LP64-006 Task 4): -DGE_COMPARE=ON defines GE_COMPARE
# and compiles and links port_compare_math.c (the in-tree tanf, atanf and cosf); no
# other build does either. In a 32-bit video comparison tree PORT_PLAY_HARNESS
# is 1 (port_playlog.h), so lockstep and the simulation trace are compiled,
# while the default 32-bit video build, the shipped kind, leaves them out.
links_object() { grep -qF -- "port/src/$2.o" "$(link_file "$1")"; }
expect_configure compare -DGE_COMPARE=ON
require_link compare
has_flag compare '-DGE_COMPARE' || fail "compare tree lacks -DGE_COMPARE"
compiles compare port_compare_math.c || fail "compare tree does not compile port_compare_math.c"
links_object compare port_compare_math.c || fail "compare tree does not link port_compare_math.c.o"
echo "ok 8: compare tree defines GE_COMPARE and links port_compare_math.c"
if has_flag default '-DGE_COMPARE'; then
    fail "default tree defines GE_COMPARE"
fi
if compiles default port_compare_math.c || links_object default port_compare_math.c; then
    fail "default tree compiles or links port_compare_math.c"
fi
echo "ok 8b: default tree does neither"
expect_failure comparelibm "GE_COMPARE_LIBM=ON needs GE_COMPARE=ON" -DGE_COMPARE_LIBM=ON
echo "ok 8c: GE_COMPARE_LIBM without GE_COMPARE is refused"
expect_configure video32 -DGE_HEADLESS=OFF -DGE_VIDEO=ON
for source in port_playlog.c port_lockstep.c port_simtrace.c port_compare_math.c; do
    if compiles video32 "$source"; then
        fail "default 32-bit video tree compiles $source"
    fi
done
expect_configure compare32video -DGE_HEADLESS=OFF -DGE_VIDEO=ON -DGE_COMPARE=ON
for source in port_playlog.c port_lockstep.c port_simtrace.c port_compare_math.c; do
    compiles compare32video "$source" || fail "32-bit video compare tree does not compile $source"
done
if cc -m32 -x c -fsyntax-only /dev/null >/dev/null 2>&1; then
    mapfile -t c_cmd < <(entry_command compare32video port/src/port_simtrace.c)
    [[ ${#c_cmd[@]} -eq 2 ]] || fail "compare32video: no port_simtrace.c compile command recorded"
    compile_canary c harness32 '#include "port_playlog.h"
#if !PORT_PLAY_HARNESS
#error PORT_PLAY_HARNESS is 0
#endif' || {
        cat "$work/canary-c-harness32.log" >&2
        fail "a 32-bit video compare tree has PORT_PLAY_HARNESS 0"
    }
    echo "ok 8d: a 32-bit video compare tree has PORT_PLAY_HARNESS 1"
else
    echo "skip 8d: cc cannot compile 32-bit code here (the 32-bit video compare tree's sources are listed: ok)"
fi
echo "Pointer-bits configure checks passed"
