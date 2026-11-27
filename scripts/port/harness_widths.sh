#!/usr/bin/env bash
# Sourced by the ROM-free test harnesses that compile with -m32, so each runs
# its body once for every pointer width the host can build and run.
#
#   widths=$(harness_widths)           # "32", "64", or "32" and "64" on lines
#   harness_flags WIDTH [EXTRA...]     # -m32 and EXTRA for 32; nothing for 64
#   harness_pending NAME WIDTH         # true while NAME WIDTH is on the list
#   harness_apple_compiler             # true when CC targets macOS
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
