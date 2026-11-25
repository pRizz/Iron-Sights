#!/usr/bin/env bash
# The 64-bit stop notice (LP64-007 Task 6, port/src/port_lp64_notice.c): the
# table of when a stop shows its line in a message box, and that a video
# build opens one box, never under GE_NOINPUT or an offscreen driver, while a
# headless build links without a box function and does nothing. The file
# compiles at 64 bits only (CMakeLists.txt), so the 32-bit half has nothing to
# build and says so.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-lp64-notice.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

build() {
    local out=$1
    shift
    "$compiler" "$@" -std=gnu11 -D_LANGUAGE_C -Wall -Wextra -Werror -iquote include -iquote port/include \
        -idirafter include scripts/port/test_lp64_notice.c port/src/port_lp64_notice.c -o "$out"
}

# Runs one test program; prints its output only on failure.
run() {
    local bin=$1 label=$2 status=0 output
    output=$("$bin" 2>&1) || status=$?
    if [[ $status != 0 ]]; then
        echo "test_lp64_notice: $label: exit $status; output:" >&2
        echo "$output" >&2
        return 1
    fi
    return 0
}

# On its own line, so a failure of harness_widths stops the harness (set -e)
# instead of running the loop zero times.
widths=$(harness_widths)
for width in $widths; do
    if [[ $width != 64 ]]; then
        echo "test_lp64_notice: ${width}-bit: nothing to build (port_lp64_notice.c compiles at 64 bits only)"
        continue
    fi
    build "$work/video"
    run "$work/video" "64-bit video build"
    build "$work/headless" -DPORT_LP64_NOTICE_HEADLESS
    run "$work/headless" "64-bit headless build"
    echo "test_lp64_notice: PASS (64-bit)"
done
