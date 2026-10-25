#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ $# -lt 2 || $# -gt 3 ]]; then
    printf 'Usage: linux-game-audit.sh local-rom output-directory [launcher|room|ui]\n' >&2
    exit 1
fi
case "${3:-launcher}" in
    launcher) audit=tools/launcher-game-audit.mjs ;;
    room) audit=tools/room-game-audit.mjs ;;
    ui) audit=tools/room-audit.mjs ;;
    *) printf 'Unknown browser audit mode\n' >&2; exit 1 ;;
esac
node tools/serve-site.mjs 4176 /Project-Midas-ADS/ &
preview_pid=$!
trap 'kill "$preview_pid"' EXIT
export MIDAS_BROWSER=${MIDAS_BROWSER:-firefox}
export MIDAS_HEADED=1
export LIBGL_ALWAYS_SOFTWARE=1
if [[ "${3:-launcher}" == ui ]]; then
    export MIDAS_AUDIT_OUTPUT="$2"
    xvfb-run -a node "$audit" http://127.0.0.1:4176/Project-Midas-ADS/
else
    xvfb-run -a node "$audit" "$1" "$2" http://127.0.0.1:4176/Project-Midas-ADS/
fi
