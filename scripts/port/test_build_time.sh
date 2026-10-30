#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-build-time.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
"${CC:-gcc}" -m32 -std=gnu11 -Wall -Wextra -Werror -Iport/include \
    scripts/port/test_build_time.c port/src/port_build_time.c -o "$test_bin"
"$test_bin"
