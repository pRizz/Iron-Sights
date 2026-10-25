#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ $# != 2 ]]; then
    printf 'Usage: linux-game-audit.sh local-rom output-directory\n' >&2
    exit 1
fi
node tools/serve-site.mjs 4176 /Project-Midas-ADS/ &
preview_pid=$!
trap 'kill "$preview_pid"' EXIT
export MIDAS_BROWSER=${MIDAS_BROWSER:-firefox}
export MIDAS_HEADED=1
export LIBGL_ALWAYS_SOFTWARE=1
xvfb-run -a node tools/launcher-game-audit.mjs "$1" "$2" http://127.0.0.1:4176/Project-Midas-ADS/
