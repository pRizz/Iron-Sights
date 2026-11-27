#!/usr/bin/env bash
# The macOS host functions (MAC-001 Task 1, port/src/port_macos.c): the main
# executable's loaded range, which the 64-bit address check reads, and the
# sleep to an absolute CLOCK_MONOTONIC time, which paces the game's frames.
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
    echo "test_macos_host: PASS ($width-bit)"
done
