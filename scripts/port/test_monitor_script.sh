#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
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

build_and_run
if "$cc" -m32 -x c -fsyntax-only /dev/null; then
    build_and_run -m32 -msse2 -mfpmath=sse
else
    echo "monitor script: -m32 unavailable on this host, skipped"
fi
