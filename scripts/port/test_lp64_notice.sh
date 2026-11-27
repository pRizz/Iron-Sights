#!/usr/bin/env bash
# The 64-bit stop notice (LP64-007 Task 6, port/src/port_lp64_notice.c): the
# table of when a stop shows its line in a message box, and that a video
# build opens one box, never under GE_NOINPUT or an offscreen driver, while a
# headless build links without a box function and does nothing. The file
# compiles at 64 bits only (CMakeLists.txt), so the 32-bit half has nothing to
# build and says so. test_lp64_stops.c runs the stops outside port_lp64.c
# (sysFatalError, the converters' allocation stops) with a printing box:
# each exits 1 with its line logged and shown, and shows none under
# GE_NOINPUT=1.
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

# The stops driver, linked as the game links these files, with a stand-in for
# the generated port_buildinfo.h that sysstubs.c includes and malloc wrapped.
build_stops() {
    local out=$1
    mkdir -p "$work/gen"
    printf '%s\n' '#define PORT_BUILD_STRING "0-test"' '#define PORT_BUILD_COMMIT "test"' \
        '#define PORT_BUILD_BRANCH ""' > "$work/gen/port_buildinfo.h"
    "$compiler" -std=gnu11 -fms-extensions -fcommon -funsigned-char \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DNONMATCHING -DAVOID_UB \
        -Wall -Wextra -Werror $(harness_game_header_flags) \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include -I"$work/gen" \
        scripts/port/test_lp64_stops.c port/src/sysstubs.c port/src/port_lp64_notice.c port/src/port_lp64.c $(harness_host_sources) \
        port/src/romconv_common.c $(harness_wrap_flags malloc) -o "$out"
}

# Runs the stops driver in MODE with NOINPUT as GE_NOINPUT (empty: unset);
# fails unless it exits 1, logs WANT_LINE, and shows it in a box exactly when
# NOINPUT is empty.
expect_stop() {
    local bin=$1 mode=$2 noinput=$3 want_line=$4 status=0 output
    if [[ -z $noinput ]]; then
        output=$(env -u GE_NOINPUT -u SDL_VIDEODRIVER "$bin" "$mode" 2>&1) || status=$?
    else
        output=$(env -u SDL_VIDEODRIVER GE_NOINPUT="$noinput" "$bin" "$mode" 2>&1) || status=$?
    fi
    local label="$mode stop (GE_NOINPUT=${noinput:-unset})"
    if [[ $status != 1 ]]; then
        echo "test_lp64_notice: $label: exit $status, expected 1; output:" >&2
        echo "$output" >&2
        return 1
    fi
    if [[ $output != *"$want_line"* ]]; then
        echo "test_lp64_notice: $label: output lacks '$want_line':" >&2
        echo "$output" >&2
        return 1
    fi
    if [[ -z $noinput && $output != *"box: Iron Sights: $want_line"* ]]; then
        echo "test_lp64_notice: $label: no box showing '$want_line':" >&2
        echo "$output" >&2
        return 1
    fi
    if [[ -n $noinput && $output == *"box: "* ]]; then
        echo "test_lp64_notice: $label: a box opened:" >&2
        echo "$output" >&2
        return 1
    fi
    return 0
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
    build_stops "$work/stops"
    for noinput in "" 1; do
        expect_stop "$work/stops" fatal "$noinput" "ge: FATAL: The bg file bg_test is not served from the ROM"
        expect_stop "$work/stops" scratch "$noinput" "ge: lp64: converter scratch: cannot allocate 2097152 bytes"
        expect_stop "$work/stops" map "$noinput" "ge: lp64: converter map: cannot reserve 4194304 bytes"
    done
    echo "test_lp64_notice: PASS (64-bit)"
done
