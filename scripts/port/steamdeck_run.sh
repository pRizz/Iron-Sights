#!/bin/sh
# Steam Deck / 32-bit-capable Linux: prefer the bundled SDL2, with system fallback.
cd "$(dirname "$0")" || exit 1
chmod +x ./IronSights 2>/dev/null
if LD_LIBRARY_PATH=. ldd ./IronSights 2>/dev/null | grep -q "not found"; then
    echo "run.sh: bundled SDL2 is missing a dependency here; using the system SDL2" >&2
    exec ./IronSights "$@"
fi
LD_LIBRARY_PATH=.
export LD_LIBRARY_PATH
exec ./IronSights "$@"
