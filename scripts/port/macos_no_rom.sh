#!/usr/bin/env bash
# With no ROM the game must stop by name, not crash or wait (MAC-001 Task 10):
# a copy of EXECUTABLE runs in an empty folder with GE_ROM unset and
# GE_NOINPUT=1, and must exit with status 3 having logged
# "ge: romdata: NO ROM". CI's macos-arm64-tests job runs this, having no ROM;
# it reads no ROM and writes nothing outside its temporary folder.
#
#   bash scripts/port/macos_no_rom.sh build/mac/headless/IronSights
set -euo pipefail
if [[ $# -ne 1 || ! -x $1 ]]; then
    echo "usage: macos_no_rom.sh EXECUTABLE (a built game)" >&2
    exit 2
fi
work=$(mktemp -d /tmp/midas-no-rom.XXXXXX)
trap 'rm -rf "$work"' EXIT
cp "$1" "$work/IronSights"

status=0
(cd "$work" && env -u GE_ROM GE_NOINPUT=1 ./IronSights >run.log 2>&1) || status=$?
if [[ $status -ne 3 ]]; then
    echo "macos_no_rom: exit status $status, expected 3; the log:" >&2
    cat "$work/run.log" >&2
    exit 1
fi
if ! grep -qF "ge: romdata: NO ROM" "$work/run.log"; then
    echo "macos_no_rom: the log lacks 'ge: romdata: NO ROM':" >&2
    cat "$work/run.log" >&2
    exit 1
fi
echo "macos_no_rom: OK (exit 3, $(grep -F "ge: romdata: NO ROM" "$work/run.log" | head -n 1 | cut -c1-60)...)"
