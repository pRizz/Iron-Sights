#!/usr/bin/env bash
# ROM-free test of Baked Lighting's ray tracer (FORK-082) on a known scene.
# The SDL shim stands in for SDL (no threads: the time-sliced path runs).
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-bake-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
"${CC:-gcc}" -std=gnu11 -Wall -Wextra -Werror -Iscripts/port/sdl_shim -Iport/src \
    scripts/port/test_baked_trace.c -lm -o "$test_bin"
"$test_bin"
