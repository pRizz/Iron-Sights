#!/usr/bin/env bash
# The repaired sites of the overlay review's defect list (LP64-005 Task 4) read
# records through their own types: ROM-free programs built with the game's
# definitions and run at each pointer width. Each includes the game file it
# tests, as test_stan_callbacks.c does, with stubs for what the tested
# functions call; -Wl,--gc-sections drops the functions not reached, so their
# callees need no stub. Every input is built in the tests from invented values.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-typed-reads.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}
tests=(chraction mipmap rocket font setup doorclear ammo player respawn)

# On its own line, so a failure of harness_widths stops the harness (set -e).
widths=$(harness_widths)
for width in $widths; do
    for name in "${tests[@]}"; do
        "$compiler" $(harness_flags "$width" -msse2 -mfpmath=sse) \
            -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -fno-strict-aliasing -w -Werror=implicit-function-declaration \
            -ffunction-sections -fdata-sections -Wl,--gc-sections \
            -DVERSION_US -DLANG_US -DREFRESH_NTSC -DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0 \
            -DNONMATCHING -DAVOID_UB -D_LANGUAGE_C \
            -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
            "scripts/port/test_lp64_typed_$name.c" -lm -o "$work/$name-$width"
        "$work/$name-$width"
    done
done
echo "test_lp64_typed_reads: PASS"
