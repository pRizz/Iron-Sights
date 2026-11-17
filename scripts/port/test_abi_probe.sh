#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
cc=${CC:-gcc}
test_bin=$(mktemp /tmp/midas-abi-probe.XXXXXX)
trap 'rm -f "$test_bin"' EXIT

# The definitions are CMakeLists.txt's add_compile_definitions for every build,
# so the layouts asserted are the game build's.
build_and_run() {
    "$cc" "$@" -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -Werror=implicit-function-declaration \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0 \
        -DNONMATCHING -DAVOID_UB -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/abi_probe/probe.c -o "$test_bin"
    "$test_bin"
}

# The probe marks gait run names no code reads (GAIT_UNREAD): at 64 bits
# they no longer overlay their Model field, so a new reader must take
# gait_model_storage's field there and be marked BRANCHED instead.
unread=$(sed -n 's/^GAIT_RUN(\([A-Za-z0-9_]*\), [A-Za-z0-9_]*, GAIT_UNREAD);$/\1/p' scripts/port/abi_probe/probe.c)
[[ -n $unread ]] || { echo "ABI probe: no GAIT_UNREAD names found" >&2; exit 1; }
pattern=$(printf '%s\n' $unread | paste -sd'|' -)
status=0
readers=$(grep -rnE --include='*.c' --include='*.cpp' --include='*.h' -e "(->|\.)($pattern)\b" src port/src) || status=$?
if [[ $status == 0 ]]; then
    echo "ABI probe: gait run names marked GAIT_UNREAD are read:" >&2
    echo "$readers" >&2
    exit 1
fi
[[ $status == 1 ]] || { echo "ABI probe: the gait run reader search failed (grep status $status)" >&2; exit 1; }

build_and_run
if "$cc" -m32 -x c -fsyntax-only /dev/null; then
    build_and_run -m32 -msse2 -mfpmath=sse
else
    echo "ABI probe: -m32 unavailable on this host, skipped"
fi
