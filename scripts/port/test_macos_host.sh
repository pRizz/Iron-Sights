#!/usr/bin/env bash
# The macOS host functions (port/src/port_macos.c): the main executable's
# loaded range, which the 64-bit address check reads, the sleep to an absolute
# CLOCK_MONOTONIC time, which paces the game's frames (MAC-001 Task 1), and the
# executable's real path, for the data root and the restart (MAC-002 Task 2).
# The file compiles only on macOS, so elsewhere this harness says so and
# passes. A Mac has no 32-bit half (harness_widths.sh).
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
if [[ $(uname -s) != Darwin ]]; then
    echo "test_macos_host: SKIP (not macOS)"
    exit 0
fi
work=$(mktemp -d /tmp/midas-macos-host.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

widths=$(harness_widths)
for width in $widths; do
    # shellcheck disable=SC2046  # harness_flags prints separate flags
    "$compiler" $(harness_flags "$width") -std=gnu11 -Wall -Wextra -Werror -Iport/include \
        scripts/port/test_macos_host.c port/src/port_macos.c -o "$work/host$width"
    "$work/host$width"
    # Through a symlink in a folder whose name has a space, the path is the
    # real file's, not the link's.
    mkdir -p "$work/a folder"
    ln -sf "$work/host$width" "$work/a folder/link$width"
    "$work/a folder/link$width" "$work/host$width"
    echo "test_macos_host: PASS ($width-bit)"
done
