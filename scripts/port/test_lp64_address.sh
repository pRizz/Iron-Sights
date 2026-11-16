#!/usr/bin/env bash
# The 64-bit address rules (LP64-003 Task 3): PORT_IS_HOST_ADDR at each width,
# portLp64RegionIsHigh, and the startup address check, which must pass in a
# position-independent executable and stop, naming the executable image, in
# one linked -no-pie (the one deliberate -no-pie link: it proves the check
# sees an image below 4 GB).
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-lp64-address.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

build() {
    local out=$1
    shift
    "$compiler" "$@" -std=gnu11 -Wall -Wextra -Werror -iquote include -iquote port/include \
        scripts/port/test_lp64_address.c port/src/port_lp64.c -o "$out"
}

# Runs the driver's startup check; fails unless it exits with WANT_STATUS and
# its output contains WANT_TEXT.
expect_address_check() {
    local bin=$1 want_status=$2 want_text=$3 label=$4 status=0 output
    output=$("$bin" address-space 2>&1) || status=$?
    if [[ $status != "$want_status" ]]; then
        echo "test_lp64_address: $label: exit $status, expected $want_status; output:" >&2
        echo "$output" >&2
        return 1
    fi
    if [[ $output != *"$want_text"* ]]; then
        echo "test_lp64_address: $label: output lacks '$want_text':" >&2
        echo "$output" >&2
        return 1
    fi
    return 0
}

# On its own line, so a failure of harness_widths stops the harness (set -e)
# instead of running the loop zero times.
widths=$(harness_widths)
for width in $widths; do
    flags=($(harness_flags "$width"))
    build "$work/unit-$width" ${flags[@]+"${flags[@]}"}
    "$work/unit-$width"
    if [[ $width == 64 ]]; then
        build "$work/pie" -fPIE -pie
        expect_address_check "$work/pie" 0 "ge: lp64: address space OK" "position-independent driver"
        build "$work/no-pie" -fno-pie -no-pie
        expect_address_check "$work/no-pie" 4 "executable image" "-no-pie driver"
    fi
    echo "test_lp64_address: PASS (${width}-bit)"
done
