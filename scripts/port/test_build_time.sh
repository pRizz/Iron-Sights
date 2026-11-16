#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-build-time.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_build_time "$width"; then
        echo "test_build_time: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    "${CC:-gcc}" $(harness_flags "$width") -std=gnu11 -Wall -Wextra -Werror -Iport/include \
        scripts/port/test_build_time.c port/src/port_build_time.c -o "$test_bin"
    "$test_bin"
    echo "test_build_time: PASS (${width}-bit)"
done
