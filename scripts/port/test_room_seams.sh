#!/usr/bin/env bash
# ROM-free test of Room Seams' triangle grow (FORK-084, port/fast3d/gfx_seam_grow.h).
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-seams-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
"${CC:-gcc}" -std=gnu11 -Wall -Wextra -Werror -Iport/fast3d scripts/port/test_room_seams.c -lm -o "$test_bin"
"$test_bin"
