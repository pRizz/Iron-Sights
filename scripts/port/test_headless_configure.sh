#!/usr/bin/env bash
# The headless build is the 32-bit reference for the 64-bit work, so it must
# configure and compile with no extra flags. Compiling one file that includes
# SDL.h keeps this fast; the full link is checked by CI's headless build, which
# the tests job runs into build/lp64/diag before the 64-bit compile gate.
set -euo pipefail
cd "$(dirname "$0")/../.."
build_dir=$(mktemp -d /tmp/midas-headless-configure.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT

# A Mac configures only the 64-bit tree (CMakeLists.txt refuses another), so
# there this checks that one, and says so.
width_args=()
width_note="32-bit"
if [[ $(uname -s) == Darwin ]]; then
    width_args=(-DGE_POINTER_BITS=64)
    width_note="64-bit: a Mac has no 32-bit build"
fi

# Quiet when it passes; a failed configure prints CMake's own diagnostics.
if ! cmake -S . -B "$build_dir" -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ${width_args[@]+"${width_args[@]}"} >"$build_dir/configure.log" 2>&1; then
    cat "$build_dir/configure.log" >&2
    exit 1
fi

# Take the recorded compile command for the file, as the build would run it.
command=$(python3 - "$build_dir/compile_commands.json" <<'PY'
import json, sys
for entry in json.load(open(sys.argv[1])):
    if entry["file"].endswith("port/src/port_baked_trace.c"):
        print(entry["command"])
        break
else:
    sys.exit("port_baked_trace.c is not in compile_commands.json")
PY
)

# The recorded command writes its object under the build tree, which this
# script removes on exit; it recorded relative paths from the build directory.
cd "$build_dir"
eval "$command"
echo "Headless configure OK: port_baked_trace.c compiles ($width_note)"
