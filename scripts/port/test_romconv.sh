#!/usr/bin/env bash
# The ROM data converters' ROM-free tests (LP64-004): each converter's test
# program, built with the game's definitions and include order and run at each
# pointer width. Every input is built inside the tests from numbers written
# there; no ROM-derived byte is read. At 32 bits a converter exists only in the
# oracle build, so the 32-bit half compiles with GE_LP64_ORACLE, which also
# gives port_lp64.c its class names. Converter sources get
# -Werror=incompatible-pointer-types, as in the game build.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-romconv.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

# Builds OUT from the test driver and the port sources after it. -Iinclude
# puts the game's own string.h first, as in the game build, hence
# -Wno-builtin-declaration-mismatch.
build() {
    local out=$1 width=$2
    shift 2
    local oracle=()
    [[ $width == 32 ]] && oracle=(-DGE_LP64_ORACLE)
    "$compiler" $(harness_flags "$width" -msse2 -mfpmath=sse) ${oracle[@]+"${oracle[@]}"} \
        -std=gnu11 -fms-extensions -fcommon -funsigned-char \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DNONMATCHING -DAVOID_UB \
        -Wall -Wextra -Werror -Werror=incompatible-pointer-types -Wno-builtin-declaration-mismatch \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        "$@" port/src/port_lp64.c -o "$out"
}

# Runs one test program; prints its output only on failure.
run() {
    local bin=$1 label=$2 status=0 output
    output=$("$bin" 2>&1) || status=$?
    if [[ $status != 0 ]]; then
        echo "test_romconv: $label: exit $status; output:" >&2
        echo "$output" >&2
        return 1
    fi
    return 0
}

# On its own line, so a failure of harness_widths stops the harness (set -e)
# instead of running the loop zero times.
widths=$(harness_widths)
for width in $widths; do
    build "$work/common-$width" "$width" scripts/port/test_romconv_common.c port/src/romconv_common.c
    run "$work/common-$width" "${width}-bit shared helpers"
    # The 64-bit ModelAnimation is packed: the sanitizers check every access
    # to it. The 32-bit half links the oracle, which romconv_anim.c calls.
    sanitize=()
    [[ $width == 64 ]] && sanitize=(-fsanitize=address,undefined -fno-sanitize-recover=all)
    build "$work/anim-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_anim.c \
        port/src/romconv_anim.c port/src/romconv_common.c port/src/port_lp64_oracle.c
    run "$work/anim-$width" "${width}-bit animation tables"
    build "$work/text-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_text.c \
        port/src/romconv_text.c port/src/romconv_common.c
    run "$work/text-$width" "${width}-bit text banks"
    bank=(scripts/port/test_romconv_bank_fixture.c port/src/romconv_bank.c port/src/romconv_bank_records.c
        port/src/romconv_bank_oracle.c port/src/romconv_common.c port/src/port_lp64_oracle.c)
    build "$work/bank-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_bank.c "${bank[@]}"
    run "$work/bank-$width" "${width}-bit audio banks"
    build "$work/bank-checks-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_bank_checks.c \
        "${bank[@]}"
    run "$work/bank-checks-$width" "${width}-bit audio bank input checks"
    echo "test_romconv: PASS (${width}-bit)"
done
