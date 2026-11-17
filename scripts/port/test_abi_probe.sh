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
# gait_model_storage's field there and be marked BRANCHED instead. This search
# finds a member access written `->name` or `.name` on one line, spaces allowed
# after the operator. It does not catch an access split across lines, a name
# built by a macro (token pasting) or reached by raw offset, and for BRANCHED
# names it checks only that a reader exists, not that every reader takes the
# 64-bit field; those are left to review.
probe=scripts/port/abi_probe/probe.c
gait_names() { # KIND: the names of the GAIT_RUN lines of that kind
    sed -nE "s/^GAIT_RUN\(([A-Za-z0-9_]+), *[A-Za-z0-9_]+, *GAIT_$1\);[[:space:]]*$/\1/p" "$probe"
}
gait_readers() { # NAME...: member accesses to any of them in src and port/src
    local pattern status=0
    pattern=$(printf '%s\n' "$@" | paste -sd'|' -)
    grep -rnE --include='*.c' --include='*.cpp' --include='*.h' -e "(->|\.)[[:space:]]*($pattern)\b" src port/src || status=$?
    [[ $status -le 1 ]] || { echo "ABI probe: the gait run reader search failed (grep status $status)" >&2; exit 1; }
}
declared=$(sed -nE 's/^#define GAIT_RUN_NAMES ([0-9]+)$/\1/p' "$probe")
lines=$(grep -c '^GAIT_RUN(' "$probe")
same=$(gait_names SAME); branched=$(gait_names BRANCHED); unread=$(gait_names UNREAD)
parsed=$(printf '%s\n' $same $branched $unread | grep -c .)
if [[ -z $declared || $lines != "$declared" || $parsed != "$declared" ]]; then
    echo "ABI probe: GAIT_RUN_NAMES is '${declared}', the probe has $lines GAIT_RUN lines and $parsed parsed" >&2
    exit 1
fi
readers=$(gait_readers $unread)
if [[ -n $readers ]]; then
    echo "ABI probe: gait run names marked GAIT_UNREAD are read:" >&2
    echo "$readers" >&2
    exit 1
fi
for name in $branched; do
    readers=$(gait_readers "$name")
    [[ -n $readers ]] || { echo "ABI probe: gait run name $name is marked GAIT_BRANCHED but nothing reads it" >&2; exit 1; }
done

build_and_run
if "$cc" -m32 -x c -fsyntax-only /dev/null; then
    build_and_run -m32 -msse2 -mfpmath=sse
else
    echo "ABI probe: -m32 unavailable on this host, skipped"
fi
