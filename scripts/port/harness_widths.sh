#!/usr/bin/env bash
# Sourced by the ROM-free test harnesses that compile with -m32, so each runs
# its body once for every pointer width the host can build and run.
#
#   widths=$(harness_widths)           # "32", "64", or "32" and "64" on lines
#   harness_flags WIDTH [EXTRA...]     # -m32 and EXTRA for 32; nothing for 64
#   harness_pending NAME WIDTH         # true while NAME WIDTH is on the list
#   harness_apple_compiler             # true when CC targets macOS
#   harness_gc_flags                   # flags that drop unreached code
#   harness_quiet_flags                # -w, and what clang needs beside it
#   harness_game_header_flags          # -Wno- classes for the game's headers
#   harness_wrap_flags FUNCTION        # route FUNCTION to __wrap_FUNCTION
#   harness_skip_unless_tool NAME TOOL # false, with a SKIP line, on a Mac without TOOL
#   harness_weak_undefined SYMBOL...   # what a link needs for weak references
#   harness_host_sources               # the host's own port sources (macOS)
#
# MIDAS_HARNESS_WIDTHS="32" or "64" restricts the widths. CC is the compiler
# asked, as in the harnesses.

# Halves that fail at that width on a defect in game or port source, as
# "NAME WIDTH" entries. The harness names each one and skips it. Empty since
# LP64-003 Task 11 made Vertex 16 bytes, which let test_wardrobe pass at 64.
HARNESS_PENDING=()

# True when the compiler targets an Apple system (its -dumpmachine names
# "-apple-", as arm64-apple-darwin27.2.0 does). A compiler that cannot say is
# not Apple's.
harness_apple_compiler() {
    local machine
    machine=$("${CC:-gcc}" -dumpmachine 2>/dev/null) || return 1
    [[ $machine == *-apple-* ]]
}

# Prints the widths to run, one per line. 32 when the compiler accepts -m32;
# 64 when the compiler's own pointers are 8 bytes (x86_64 and aarch64 use SSE
# or NEON arithmetic by default, so 64 needs no flag). An Apple compiler is
# never offered 32, whatever it says to -m32. Fails, naming what was
# asked, when none can run; on an x86 host a rejected -m32 is a failure too
# (the 32-bit toolchain is missing) unless MIDAS_HARNESS_WIDTHS leaves 32 out.
harness_widths() {
    local cc=${CC:-gcc} requested=${MIDAS_HARNESS_WIDTHS:-"32 64"} width macros machine found=0
    for width in $requested; do
        case $width in
        32)
            if harness_apple_compiler; then
                # Apple clang accepts -m32 for arm64 and then can compile and
                # run nothing with it: a Mac has no 32-bit half (MAC-001).
                :
            elif "$cc" -m32 -x c -fsyntax-only /dev/null 2>/dev/null; then
                echo 32
                found=1
            else
                # A compiler that cannot make 32-bit code at all (aarch64)
                # just offers no 32. On an x86 host the missing piece is the
                # 32-bit toolchain, which has always failed a harness.
                if ! machine=$("$cc" -dumpmachine 2>&1); then
                    echo "harness_widths: $cc -dumpmachine failed:" >&2
                    echo "$machine" >&2
                    return 1
                fi
                case $machine in
                x86_64* | i[3-6]86*)
                    echo "harness_widths: $cc ($machine) rejects -m32: the 32-bit toolchain is missing" \
                        "(install gcc-multilib), or run only the 64-bit half with MIDAS_HARNESS_WIDTHS=64" >&2
                    return 1
                    ;;
                esac
            fi
            ;;
        64)
            # Read the macros whole: "grep -q" closing the pipe early made the
            # compiler fail with SIGPIPE, and pipefail then dropped this width.
            if ! macros=$("$cc" -E -dM -x c /dev/null 2>&1); then
                echo "harness_widths: $cc -E -dM failed:" >&2
                echo "$macros" >&2
                return 1
            fi
            if [[ $macros == *$'__SIZEOF_POINTER__ 8\n'* || $macros == *'__SIZEOF_POINTER__ 8' ]]; then
                echo 64
                found=1
            fi
            ;;
        *)
            echo "harness_widths: unknown width '$width' (use 32 or 64)" >&2
            return 2
            ;;
        esac
    done
    if [[ $found == 0 ]]; then
        echo "harness_widths: $cc cannot build any of the requested widths ($requested)" >&2
        return 1
    fi
    return 0
}

# Prints the compiler flags that select WIDTH: -m32 followed by EXTRA (the
# harness's own x86-only flags, such as -msse2 -mfpmath=sse) for 32, nothing
# for 64.
harness_flags() {
    local width=$1
    shift
    if [[ $width == 32 ]]; then
        echo -m32 "$@"
    fi
    return 0
}

# True when "NAME WIDTH" is in HARNESS_PENDING.
harness_pending() {
    local entry
    for entry in "${HARNESS_PENDING[@]}"; do
        if [[ $entry == "$1 $2" ]]; then
            return 0
        fi
    done
    return 1
}

# True when the compiler is clang (Apple's or LLVM's), by its --version.
harness_clang_compiler() {
    local version
    version=$("${CC:-gcc}" --version 2>/dev/null) || return 1
    [[ $version == *clang* ]]
}

# Prints the flags that let a harness link a few functions of a game file
# without the rest of the game: the linker drops code nothing reaches, and
# with it that code's undefined references. GNU ld does so by section; Apple's
# linker by symbol, with its own flag.
harness_gc_flags() {
    if harness_apple_compiler; then
        echo -Wl,-dead_strip
    else
        echo -ffunction-sections -fdata-sections -Wl,--gc-sections
    fi
    return 0
}

# Prints the flags that silence a harness's compile of game code: -w, as the
# 32-bit game build has. GCC's -w also silences an implicit function
# declaration and an int/pointer conversion; clang makes those two errors,
# which -w does not reach, so there they are made warnings again first. Put
# this after any -Werror= the command names: the later flag wins on clang.
harness_quiet_flags() {
    if harness_clang_compiler; then
        echo -w -Wno-error=implicit-function-declaration -Wno-error=int-conversion
    else
        echo -w
    fi
    return 0
}

# Prints the flags that let a -Wall -Wextra -Werror compile include the
# game's headers. Its own include/string.h declares strchr and strlen unlike
# the compiler's built-in ones: GCC and clang each have a class for that, and
# clang refuses GCC's name. Clang also warns of three things GCC accepts
# silently and the game build switches off by name (CMakeLists.txt):
# anonymous struct members, the game's own ntohl and ntohs, and a header
# that leaves #pragma pack changed.
harness_game_header_flags() {
    if harness_clang_compiler; then
        echo -Wno-incompatible-library-redeclaration -Wno-microsoft-anon-tag -Wno-macro-redefined -Wno-pragma-pack
    else
        echo -Wno-builtin-declaration-mismatch
    fi
    return 0
}

# Prints the flags that send every call of FUNCTION in the harness's sources
# to __wrap_FUNCTION, which the driver defines and which reaches the real one
# as __real_FUNCTION. GNU ld does this at link time. Apple's linker cannot,
# so there the compiler renames the calls, and the driver defines
# __real_FUNCTION itself (under __APPLE__).
harness_wrap_flags() {
    if harness_apple_compiler; then
        echo "-D$1=__wrap_$1"
    else
        echo "-Wl,--wrap=$1"
    fi
    return 0
}

# On a Mac without TOOL on the PATH, prints "NAME: SKIP (TOOL not installed)"
# and returns 1, so a harness that cannot run there says so by name instead
# of failing or passing silently. Returns 0 everywhere else: off macOS a
# missing tool is no reason to skip, and the harness goes on to fail on it
# (CI's browser job runs test_web_touch and must not pass without emcc).
harness_skip_unless_tool() {
    if [[ $(uname -s) != Darwin ]] || command -v "$2" >/dev/null 2>&1; then
        return 0
    fi
    echo "$1: SKIP ($2 not installed)"
    return 1
}

# Prints the link flags that let each SYMBOL, a weak reference the harness
# leaves undefined (code that tests "portX != NULL"), stay undefined. ELF
# makes such a reference null without a flag; Apple's linker must be told
# each one by name, which keeps any other undefined symbol an error.
harness_weak_undefined() {
    local symbol flags=()
    if harness_apple_compiler; then
        for symbol in "$@"; do
            flags+=("-Wl,-U,_$symbol")
        done
        echo "${flags[@]}"
    fi
    return 0
}

# Prints the port sources only this host compiles, for a harness that links
# port files calling them: port/src/port_macos.c on an Apple compiler
# (pcsched.c's sleep, port_lp64.c's image range), nothing elsewhere.
harness_host_sources() {
    if harness_apple_compiler; then
        echo port/src/port_macos.c
    fi
    return 0
}
