#!/usr/bin/env bash
# ROM-free regression (LP64-007 Task 2): portBuildArchitecture() returns
# "i686" for a 32-bit x86 build and "x86_64" or "aarch64" for a 64-bit one.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-build-arch.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    "${CC:-gcc}" $(harness_flags "$width") -std=gnu11 -Wall -Wextra -Werror -Iport/include \
        scripts/port/test_build_arch.c -o "$test_bin"
    "$test_bin"
    echo "test_build_arch: PASS (${width}-bit)"
done
