#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$here"
# ZIP extractors may drop executable permissions.
chmod +x ./IronSights ./sentry-crash 2>/dev/null || true
export LD_LIBRARY_PATH="$here/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec ./IronSights "$@"
