#!/usr/bin/env bash
# The in-game updater's parsing, archives, installs, rollbacks and refusals
# (FORK-095 UPD-002) at each pointer width, against update_fixture.py's fake
# releases. Needs python3; no ROM and no network.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-update.XXXXXX)
trap 'rm -rf "$work"' EXIT
widths=$(harness_widths)
for width in $widths; do
    rm -rf "$work/fixture"
    python3 scripts/port/update_fixture.py "$work/fixture"
    # puff is upstream zlib code, built as the game builds it.
    "${CC:-gcc}" $(harness_flags "$width") $(harness_quiet_flags) -c port/fast3d/third_party/puff/puff.c \
        -o "$work/puff.o"
    # Large-file stat, as CMake gives port_update_files.c in a 32-bit Linux build.
    "${CC:-gcc}" $(harness_flags "$width") -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 \
        -std=gnu11 -Wall -Wextra -Werror -Iport/include -Iport/src \
        scripts/port/test_update.c port/src/port_update.c port/src/port_update_core.c \
        port/src/port_update_archive.c port/src/port_update_files.c port/src/port_update_sha256.c \
        port/src/port_update_swap.c "$work/puff.o" -o "$work/test_update"
    if ! "$work/test_update" "$work/fixture" > "$work/log.txt" 2>&1; then
        cat "$work/log.txt"
        exit 1
    fi
    echo "test_update: PASS (${width}-bit)"
done
