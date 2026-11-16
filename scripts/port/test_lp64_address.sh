#!/usr/bin/env bash
# The 64-bit address rules (LP64-003 Task 3): PORT_IS_HOST_ADDR at each width,
# portLp64RegionIsHigh, and the startup address check, which must pass in a
# position-independent executable and stop, naming the executable image, in
# one linked -no-pie (the one deliberate -no-pie link: it proves the check
# sees an image below 4 GB). The asset-class guard (Task 4): at 64 bits it
# stops with status 5 naming the class, at 32 bits it returns. The command-word
# predicates (Task 6): PORT_WORD_IS_HOST, PORT_WORD_HAS_SEGMENT and
# PORT_WORD_IS_INDEX at each width, and port_textures.c's external-texture slot,
# which must hold the whole pixel address (above 4 GB at 64 bits). The
# command-word accessors (Task 7): GFX_OP of a gbi.h-built command, and a slot
# of texLoad's parameter type that keeps every byte of a 64-bit value.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-lp64-address.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

# gbi.h (Task 7) includes <PR/...>; -idirafter finds those after the system
# headers, so the driver's <string.h> is still the C library's.
build() {
    local out=$1
    shift
    "$compiler" "$@" -std=gnu11 -D_LANGUAGE_C -Wall -Wextra -Werror -iquote include -iquote port/include \
        -idirafter include scripts/port/test_lp64_address.c port/src/port_lp64.c -o "$out"
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

# Runs the driver with MODE (address-space or require-animations); fails unless
# it exits with WANT_STATUS and its output contains WANT_TEXT.
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
    build "$work/unit-$width" ${flags[@]+"${flags[@]}"}
    "$work/unit-$width"
    build_textures "$work/textures-$width" ${flags[@]+"${flags[@]}"}
    expect_textures "$work/textures-$width" "$work/root-$width" "${width}-bit external-texture slot"
    if [[ $width == 64 ]]; then
        build "$work/pie" -fPIE -pie
        expect_driver "$work/pie" address-space 0 "ge: lp64: address space OK" "position-independent driver"
        build "$work/no-pie" -fno-pie -no-pie
        expect_driver "$work/no-pie" address-space 4 "executable image" "-no-pie driver"
        expect_driver "$work/unit-$width" require-animations 5 \
            'ge: lp64: stopping: asset class "animation tables" has no 64-bit converter yet (LP64-004)' \
            "64-bit animation guard"
    else
        expect_driver "$work/unit-$width" require-animations 0 "require-animations: returned" "32-bit animation guard"
    fi
    echo "test_lp64_address: PASS (${width}-bit)"
done
