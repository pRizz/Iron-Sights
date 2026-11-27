#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
cc=${CC:-gcc}
test_bin=$(mktemp /tmp/midas-monitor-script.XXXXXX)
trap 'rm -f "$test_bin"' EXIT

# Same language flags and definitions as scripts/port/test_abi_probe.sh, so the
# monitor structs and script macros are the game build's.
build_and_run() {
    "$cc" "$@" -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -Werror=implicit-function-declaration \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0 \
        -DNONMATCHING -DAVOID_UB -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_monitor_script.c -o "$test_bin"
    "$test_bin"
}

# On its own line, so a failure of harness_widths stops the harness (set -e).
widths=$(harness_widths)
for width in $widths; do
    # shellcheck disable=SC2046 # harness_flags prints separate flags
    build_and_run $(harness_flags "$width" -msse2 -mfpmath=sse)
    echo "test_monitor_script: PASS (${width}-bit)"
done
