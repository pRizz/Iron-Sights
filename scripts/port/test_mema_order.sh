#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
cc=${CC:-gcc}
test_bin=$(mktemp /tmp/midas-mema-order.XXXXXX)
trap 'rm -f "$test_bin"' EXIT

# Same language flags and definitions as scripts/port/test_monitor_script.sh;
# the driver includes src/mema.c, so it runs the allocator the game builds.
# -fwrapv as in the game build (CMakeLists.txt): at 32 bits _memaFree's slot
# index wraps for these addresses, which is defined only with it.
build_and_run() {
    "$cc" "$@" -std=gnu11 -fms-extensions -fcommon -funsigned-char -fwrapv -Werror=implicit-function-declaration \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -DNONMATCHING -DAVOID_UB -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_mema_order.c -o "$test_bin"
    "$test_bin"
}

build_and_run
if "$cc" -m32 -x c -fsyntax-only /dev/null; then
    build_and_run -m32 -msse2 -mfpmath=sse
else
    echo "mema order: -m32 unavailable on this host, skipped"
fi
