#!/usr/bin/env bash
# The 64-bit address rules (LP64-003 Task 3): PORT_IS_HOST_ADDR at each width,
# portLp64RegionIsHigh, and the startup address check, which must pass in a
# position-independent executable and stop, naming the executable image, in
# one linked -no-pie (the one deliberate -no-pie link: it proves the check
# sees an image below 4 GB). The asset-class guard (Task 4): every class is
# converted (LP64-004 Task 15), so it returns at both widths; at 64 bits a
# driver built with -DGE_LP64_TEST_UNCONVERTED naming a class stops there with
# status 5 naming the class. The command-word
# predicates (Task 6): PORT_WORD_IS_HOST, PORT_WORD_HAS_SEGMENT and
# PORT_WORD_IS_INDEX at each width, and port_textures.c's external-texture slot,
# which must hold the whole pixel address (above 4 GB at 64 bits). The
# command-word accessors (Task 7): GFX_OP of a gbi.h-built command, and a slot
# of texLoad's parameter type that keeps every byte of a 64-bit value. The
# game's strtol and gu.h's FTOFIX32 (Task 10): their results at 64 bits are the
# 32-bit ones. The pool reports (LP64-004 Task 2): at 64 bits a pool report
# prints at most eight lines a pool a stage, portLp64PoolFatal and the music
# heap's allocator asked for more than its heap holds stop with status 6
# naming the pool; at 32 bits they return. The bank start check (LP64-005
# Task 12): at 64 bits a bank whose start is not 8-aligned stops the game
# naming the bank, and an aligned one returns.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-lp64-address.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

# The game's src/str.c with the game's definitions, so the driver's strtol is
# the game's. -w: the driver's warning flags are not for upstream code.
build_str() {
    local out=$1
    shift
    "$compiler" "$@" -c -std=gnu11 -fms-extensions -fcommon -funsigned-char -w \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DNONMATCHING -DAVOID_UB \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        src/str.c -o "$out"
}

# The music heap's allocator (src/libultra/audio/heapalloc.c), compiled as the
# game compiles it, so its pool report is the game's call.
build_heap() {
    local out=$1
    shift
    "$compiler" "$@" -c -std=gnu11 -fms-extensions -fcommon -funsigned-char -w \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DNONMATCHING -DAVOID_UB \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        src/libultra/audio/heapalloc.c -o "$out"
}

# The stage bank's allocator (port/src/port_memp.c), compiled as the game
# compiles it, for the bank start check; 64 bits only, where the check exists.
build_memp() {
    local out=$1
    shift
    "$compiler" "$@" -c -std=gnu11 -fms-extensions -fcommon -funsigned-char -w \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DNONMATCHING -DAVOID_UB \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        port/src/port_memp.c -o "$out"
}

# gbi.h (Task 7) includes <PR/...>; -idirafter finds those after the system
# headers, so the driver's <string.h> is still the C library's.
build() {
    local out=$1 str_object=$2 heap_object=$3 memp_object=$4
    shift 4
    "$compiler" "$@" -std=gnu11 -D_LANGUAGE_C -Wall -Wextra -Werror -iquote include -iquote port/include \
        -iquote src -idirafter include scripts/port/test_lp64_address.c port/src/port_lp64.c "$str_object" \
        "$heap_object" ${memp_object:+"$memp_object"} -o "$out"
}

# The external-texture slot (Task 6): port_textures.c linked with the stubs in
# test_lp64_textures.c. -Iinclude puts the game's own string.h first, as in the
# game build, hence -Wno-builtin-declaration-mismatch.
build_textures() {
    local out=$1
    shift
    "$compiler" "$@" -std=gnu11 -fms-extensions -fcommon -funsigned-char \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DNONMATCHING -DAVOID_UB \
        -Wall -Wextra -Werror -Wno-builtin-declaration-mismatch \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_lp64_textures.c port/src/port_textures.c -o "$out"
}

# Runs the texture driver in an empty root; prints its output only on failure.
expect_textures() {
    local bin=$1 root=$2 label=$3 status=0 output
    mkdir "$root"
    output=$("$bin" "$root" 2>&1) || status=$?
    if [[ $status != 0 ]]; then
        echo "test_lp64_address: $label: exit $status; output:" >&2
        echo "$output" >&2
        return 1
    fi
    return 0
}

# Runs the driver with MODE (address-space, require-animations, require-setup,
# pool-fatal, music-heap, bank-start-aligned or bank-start-misaligned); fails unless it exits with WANT_STATUS and its
# output contains WANT_TEXT.
expect_driver() {
    local bin=$1 mode=$2 want_status=$3 want_text=$4 label=$5 status=0 output
    output=$("$bin" "$mode" 2>&1) || status=$?
    if [[ $status != "$want_status" ]]; then
        echo "test_lp64_address: $label: exit $status, expected $want_status; output:" >&2
        echo "$output" >&2
        return 1
    fi
    if [[ $output != *"$want_text"* ]]; then
        echo "test_lp64_address: $label: output lacks '$want_text':" >&2
        echo "$output" >&2
        return 1
    fi
    return 0
}

# On its own line, so a failure of harness_widths stops the harness (set -e)
# instead of running the loop zero times.
widths=$(harness_widths)
for width in $widths; do
    flags=($(harness_flags "$width"))
    build_str "$work/str-$width.o" ${flags[@]+"${flags[@]}"}
    build_heap "$work/heap-$width.o" ${flags[@]+"${flags[@]}"}
    memp=""
    if [[ $width == 64 ]]; then
        memp="$work/memp-$width.o"
        build_memp "$memp" ${flags[@]+"${flags[@]}"}
    fi
    build "$work/unit-$width" "$work/str-$width.o" "$work/heap-$width.o" "$memp" ${flags[@]+"${flags[@]}"}
    "$work/unit-$width"
    build_textures "$work/textures-$width" ${flags[@]+"${flags[@]}"}
    expect_textures "$work/textures-$width" "$work/root-$width" "${width}-bit external-texture slot"
    if [[ $width == 64 ]]; then
        build "$work/pie" "$work/str-$width.o" "$work/heap-$width.o" "$memp" -fPIE -pie
        expect_driver "$work/pie" address-space 0 "ge: lp64: address space OK" "position-independent driver"
        build "$work/no-pie" "$work/str-$width.o" "$work/heap-$width.o" "$memp" -fno-pie -no-pie
        expect_driver "$work/no-pie" address-space 4 "executable image" "-no-pie driver"
        expect_driver "$work/unit-$width" require-animations 0 "require-animations: returned" \
            "64-bit animation guard (converted)"
        expect_driver "$work/unit-$width" require-setup 0 "require-setup: returned" \
            "64-bit setup files guard (converted)"
        # A test-only build keeps one class unconverted, so the status-5 stop
        # that scripts/port/lp64_boot.py --expect-stop looks for stays tested.
        build "$work/unconverted" "$work/str-$width.o" "$work/heap-$width.o" "$memp" ${flags[@]+"${flags[@]}"} \
            -DGE_LP64_TEST_UNCONVERTED=PORT_LP64_SETUPS
        expect_driver "$work/unconverted" require-setup 5 \
            'ge: lp64: stopping: asset class "setup files" is marked unconverted in this build' \
            "64-bit guard of a class a test build leaves unconverted"
        expect_driver "$work/unit-$width" pool-fatal 6 \
            'ge: lp64: pool "music heap" exhausted (request 4096 bytes)' "64-bit fatal pool report"
        expect_driver "$work/unit-$width" music-heap 6 \
            'ge: lp64: pool "music heap" exhausted (request 4096 bytes)' "64-bit music heap exhausted"
        expect_driver "$work/unit-$width" bank-start-aligned 0 "bank-start-aligned: returned" \
            "64-bit bank start check (8-aligned)"
        expect_driver "$work/unit-$width" bank-start-misaligned 1 \
            'ge: FATAL: Memory bank 4 "stage" starts at an address that is not 8-aligned' \
            "64-bit bank start check (misaligned)"
    else
        expect_driver "$work/unit-$width" require-animations 0 "require-animations: returned" "32-bit animation guard"
        expect_driver "$work/unit-$width" require-setup 0 "require-setup: returned" "32-bit setup files guard"
        expect_driver "$work/unit-$width" pool-fatal 0 "pool-fatal: returned" "32-bit fatal pool report"
        expect_driver "$work/unit-$width" music-heap 0 "music-heap: returned NULL" "32-bit music heap exhausted"
    fi
    echo "test_lp64_address: PASS (${width}-bit)"
done
