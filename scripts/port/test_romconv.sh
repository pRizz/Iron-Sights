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
    build "$work/seq-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_seq.c \
        port/src/romconv_seq.c port/src/romconv_common.c port/src/port_lp64_oracle.c
    run "$work/seq-$width" "${width}-bit music sequences"
    build "$work/gfx-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_gfx.c \
        port/src/romconv_gfx.c port/src/romconv_common.c port/src/port_lp64_oracle.c
    run "$work/gfx-$width" "${width}-bit display-list expanders"
    font=(scripts/port/test_romconv_font_fixture.c port/src/romconv_font.c port/src/romconv_gfx.c
        port/src/romconv_common.c port/src/port_lp64_oracle.c)
    build "$work/font-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_font.c "${font[@]}"
    run "$work/font-$width" "${width}-bit fonts"
    build "$work/font-checks-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_font_checks.c \
        "${font[@]}"
    run "$work/font-checks-$width" "${width}-bit font input checks"
    # At 32 bits the model tests compare with romfile_model.c's in-place
    # result. Its field map for opcode 17 is unused (the walk swaps that
    # record with the group map), which -Wextra reports.
    model=(port/src/romconv_model.c port/src/romconv_model_walk.c port/src/romconv_model_refs.c
        port/src/romconv_model_records.c port/src/romconv_gfx.c port/src/romconv_census.c port/src/romconv_common.c
        port/src/port_lp64_oracle.c)
    [[ $width == 32 ]] && model+=(-Wno-unused-const-variable port/src/romfile_model.c)
    build "$work/model-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_model.c "${model[@]}"
    run "$work/model-$width" "${width}-bit model files"
    build "$work/model-checks-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} \
        scripts/port/test_romconv_model_checks.c "${model[@]}"
    run "$work/model-checks-$width" "${width}-bit model file input checks"
    build "$work/model-shapes-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} \
        scripts/port/test_romconv_model_shapes.c "${model[@]}"
    run "$work/model-shapes-$width" "${width}-bit model file shapes"
    # The census lines: the model converter's against romfile_model.c's own,
    # at both widths, and the stan converter's.
    census=("${model[@]}" port/src/romconv_stan.c)
    [[ $width == 64 ]] && census+=(-Wno-unused-const-variable port/src/romfile_model.c)
    build "$work/census-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_census.c "${census[@]}"
    run "$work/census-$width" "${width}-bit census lines"
    bg=(scripts/port/test_romconv_bg_fixture.c port/src/romconv_bg.c port/src/romconv_gfx.c
        port/src/romconv_common.c port/src/port_lp64_oracle.c)
    build "$work/bg-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_bg.c "${bg[@]}"
    run "$work/bg-$width" "${width}-bit background tables"
    build "$work/bg-checks-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_bg_checks.c \
        "${bg[@]}"
    run "$work/bg-checks-$width" "${width}-bit background input checks and room lists"
    # At 32 bits the collision tile tests compare with romfile_stan.c's
    # in-place result.
    stan=(port/src/romconv_stan.c port/src/romconv_common.c port/src/port_lp64_oracle.c)
    [[ $width == 32 ]] && stan+=(port/src/romfile_stan.c)
    build "$work/stan-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_stan.c "${stan[@]}"
    run "$work/stan-$width" "${width}-bit collision tiles"
    # At 32 bits the setup tests compare with romfile_setup.c's in-place
    # result. Its header comment names assets/obseg/setup/*.c, which
    # -Wcomment reports.
    setup=(port/src/romconv_setup.c port/src/romconv_setup_walk.c port/src/romconv_setup_records.c
        port/src/romconv_setup_intro.c port/src/romconv_setup_props.c port/src/romconv_census.c
        port/src/romconv_common.c port/src/port_lp64_oracle.c)
    [[ $width == 32 ]] && setup+=(-Wno-comment port/src/romfile_setup.c)
    # The main setup test compares the log line with romfile_setup.c's at both widths.
    setup_main=("${setup[@]}")
    [[ $width == 64 ]] && setup_main+=(-Wno-comment port/src/romfile_setup.c)
    build "$work/setup-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_setup.c \
        "${setup_main[@]}"
    run "$work/setup-$width" "${width}-bit setup files"
    build "$work/setup-checks-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_setup_checks.c \
        "${setup[@]}"
    run "$work/setup-checks-$width" "${width}-bit setup file input checks"
    # bondview.h, for textoverride, has a comment -Wcomment reports.
    build "$work/setup-props-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} -Wno-comment \
        scripts/port/test_romconv_setup_props.c "${setup[@]}"
    run "$work/setup-props-$width" "${width}-bit setup file prop records"
    # The intro test links its own prop converter double instead of the prop
    # records' converter.
    setup_intro=()
    for source in "${setup[@]}"; do
        [[ $source == port/src/romconv_setup_props.c ]] || setup_intro+=("$source")
    done
    build "$work/setup-intro-$width" "$width" ${sanitize[@]+"${sanitize[@]}"} scripts/port/test_romconv_setup_intro.c \
        "${setup_intro[@]}"
    run "$work/setup-intro-$width" "${width}-bit setup file intro of every kind"
    echo "test_romconv: PASS (${width}-bit)"
done
