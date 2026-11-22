#!/usr/bin/env bash
# ROM-free regression (LP64-006 Task 8): a GE_COMPARE build draws the same
# legal-screen and file-select build lines whatever build number, commit, time
# and modified flag its tree baked in; a shipped build still draws them.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-startup-stamp-test.XXXXXX)
trap 'rm -rf "${work:?}"' EXIT
compiler=${CC:-gcc}

# Writes a port_buildinfo.h into DIR with build NUMBER, COMMIT, TIME and DIRTY.
write_buildinfo() {
    mkdir -p "$1"
    cat > "$1/port_buildinfo.h" <<INFO
#define PORT_BUILD_STRING "$2"
#define PORT_BUILD_COMMIT "$3"
#define PORT_BUILD_TIME "$4"
#define PORT_BUILD_DIRTY $5
INFO
}

write_buildinfo "$work/one" "597-20261122" "d1170ef17" "2026-11-22T10:01:40Z" 0
write_buildinfo "$work/two" "598-20261123" "5e46a1f00" "2026-11-23T04:09:51Z" 1

# Prints the lines a build with build information DIR and EXTRA defines draws.
draw_lines() {
    local width=$1 info=$2
    shift 2
    local flags=($(harness_flags "$width"))
    # port_build_time.c is hosted code: the game's include/ would hide libc's string.h.
    "$compiler" "${flags[@]}" -std=gnu11 -w -O0 -Iport/include -c port/src/port_build_time.c -o "$work/time.o"
    "$compiler" "${flags[@]}" -std=gnu11 -fms-extensions -funsigned-char -w -O0 "$@" \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DNONMATCHING \
        -I"$info" -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_startup_stamp.c port/src/port_startup.c "$work/time.o" -o "$work/bin"
    TZ=UTC "$work/bin"
}

for width in $(harness_widths); do
    shipped_one=$(draw_lines "$width" "$work/one")
    shipped_two=$(draw_lines "$width" "$work/two")
    if [[ "$shipped_one" == "$shipped_two" ]] || [[ "$shipped_one" != *d1170ef17* ]]; then
        echo "test_startup_stamp: FAIL (${width}-bit): a shipped build no longer draws its build lines" >&2
        exit 1
    fi
    compare_one=$(draw_lines "$width" "$work/one" -DGE_COMPARE=1)
    compare_two=$(draw_lines "$width" "$work/two" -DGE_COMPARE=1)
    if [[ "$compare_one" != "$compare_two" ]]; then
        echo "test_startup_stamp: FAIL (${width}-bit): comparison builds draw their build information:" >&2
        diff <(echo "$compare_one") <(echo "$compare_two") >&2 || true
        exit 1
    fi
    echo "test_startup_stamp: PASS (${width}-bit)"
done
