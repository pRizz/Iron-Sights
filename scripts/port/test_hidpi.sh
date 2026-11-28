#!/usr/bin/env bash
# The points-and-pixels helpers (MAC-002 Task 6, port/src/port_hidpi.c): on a
# Retina display a window's size and the mouse are in points while the
# renderer's framebuffer is in pixels, and a screenshot's rows are as wide as
# the drawable. ROM-free, no game header; runs at every width the host offers.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-hidpi.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

widths=$(harness_widths)
for width in $widths; do
    # shellcheck disable=SC2046  # harness_flags prints separate flags
    "$compiler" $(harness_flags "$width") -std=gnu11 -Wall -Wextra -Werror -Iport/include \
        scripts/port/test_hidpi.c port/src/port_hidpi.c -o "$work/hidpi$width"
    "$work/hidpi$width"
    echo "test_hidpi: PASS ($width-bit)"
done
