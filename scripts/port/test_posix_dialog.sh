#!/usr/bin/env bash
# FORK-092: port/fast3d/posix_dialog.cpp, the zenity / kdialog runner of the
# Linux save transfer dialogs, against fake tools written here: a chosen file,
# a tool that crashes after answering, Cancel, the kdialog fallback, no tool,
# and the GL switches in the tool's environment. Runs at every width
# harness_widths offers.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-posix-dialog.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CXX:-g++}

tool() { # tool DIR NAME BODY
    mkdir -p "$work/bin/$1"
    printf '#!/bin/sh\n%s\n' "$3" > "$work/bin/$1/$2"
    chmod +x "$work/bin/$1/$2"
}
tool ok zenity 'for a; do last=$a; done; echo "${last#--filename=}"'
tool crash zenity '/bin/sleep 0.3; echo "$1"; kill -SEGV $$'
tool cancel zenity 'exit 1'
tool kdeonly kdialog 'echo "kdialog $*"'
tool env zenity 'echo "$GDK_DEBUG $GDK_DISABLE"'
mkdir -p "$work/bin/none"

# On its own line, so a failure of harness_widths stops the harness (set -e).
widths=$(harness_widths)
for width in $widths; do
    # shellcheck disable=SC2046 # harness_flags prints separate flags
    "$compiler" $(harness_flags "$width") -std=c++20 -Wall -Wextra -Werror -Iport/fast3d \
        scripts/port/test_posix_dialog.cpp port/fast3d/posix_dialog.cpp -o "$work/test-$width"
    status=0
    output=$("$work/test-$width" "$work/bin" 2>&1) || status=$?
    if [[ $status != 0 ]]; then
        echo "test_posix_dialog: ${width}-bit: exit $status; output:" >&2
        echo "$output" >&2
        exit 1
    fi
    echo "test_posix_dialog: PASS (${width}-bit): $output"
done
