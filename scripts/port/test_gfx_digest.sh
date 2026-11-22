#!/usr/bin/env bash
# The ROM-free checks of the picture digest (LP64-006 Task 7) at each pointer
# width the host can build: test_gfx_digest.cpp includes
# port/src/port_gfx_digest.cpp and drives it through a stand-in rendering API.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-gfx-digest.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CXX:-g++}

# On its own line, so a failure of harness_widths stops the harness (set -e).
widths=$(harness_widths)
for width in $widths; do
    flags=($(harness_flags "$width" -msse2 -mfpmath=sse) -std=c++20 -Wall -Wextra -Werror -Iport/include)
    "$compiler" "${flags[@]}" scripts/port/test_gfx_digest.cpp -o "$work/gfx-digest-$width"
    "$work/gfx-digest-$width"
done
echo "test_gfx_digest: PASS"
