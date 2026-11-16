#!/usr/bin/env bash
# Sourced by the ROM-free test harnesses that compile with -m32, so each runs
# its body once for every pointer width the host can build and run.
#
#   widths=$(harness_widths)           # "32", "64", or "32" and "64" on lines
#   harness_flags WIDTH [EXTRA...]     # -m32 and EXTRA for 32; nothing for 64
#   harness_pending NAME WIDTH         # true while NAME WIDTH is on the list
#
# MIDAS_HARNESS_WIDTHS="32" or "64" restricts the widths. CC is the compiler
# asked, as in the harnesses.

# Halves that fail at that width on a defect in game or port source, as
# "NAME WIDTH" entries. The harness names each one and skips it. LP64-003 Task
# 11 empties the list.
HARNESS_PENDING=(
    "test_ads 64"
    "test_wardrobe 64"
)

# Prints the widths to run, one per line. 32 when the compiler accepts -m32;
# 64 when the compiler's own pointers are 8 bytes (x86_64 and aarch64 use SSE
# or NEON arithmetic by default, so 64 needs no flag). Fails, naming what was
# asked, when none can run.
harness_widths() {
    local cc=${CC:-gcc} requested=${MIDAS_HARNESS_WIDTHS:-"32 64"} width macros found=0
    for width in $requested; do
        case $width in
        32)
            if "$cc" -m32 -x c -fsyntax-only /dev/null 2>/dev/null; then
                echo 32
                found=1
            fi
            ;;
        64)
            # Read the macros whole: "grep -q" closing the pipe early made the
            # compiler fail with SIGPIPE, and pipefail then dropped this width.
            # A compiler that fails offers no 64 here, and the error below
            # names the request when that leaves no width at all.
            macros=$("$cc" -E -dM -x c /dev/null 2>/dev/null || true)
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
