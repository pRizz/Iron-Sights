#!/usr/bin/env bash
# The room cache's part key at each pointer width (LP64-005 Task 11).
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-roomcache-key.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    "${CC:-gcc}" $(harness_flags "$width") -std=gnu11 -Wall -Werror -I. -Iinclude \
        scripts/port/test_roomcache_key.c -o "$test_bin"
    "$test_bin"
done
