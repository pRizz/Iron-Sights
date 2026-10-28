#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-web-menu-pointer.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror scripts/port/test_web_menu_pointer.c -o "$test_bin"
"$test_bin"
