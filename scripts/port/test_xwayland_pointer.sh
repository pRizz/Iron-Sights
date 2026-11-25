#!/usr/bin/env bash
# The XWayland pointer reset on focus (FORK-091, port/src/port_xwayland_pointer.c):
# which focus changes reset the cursor, and that a reset shows, then hides, it.
# Compiled against a stand-in SDL.h written here, which declares only the two
# calls the module makes; test_xwayland_pointer.c defines them. Runs at every
# width harness_widths offers.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-xwayland-pointer.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

mkdir -p "$work/sdl"
cat > "$work/sdl/SDL.h" <<'EOF'
#define SDL_QUERY -1
#define SDL_DISABLE 0
#define SDL_ENABLE 1
int SDL_ShowCursor(int toggle);
const char *SDL_GetCurrentVideoDriver(void);
EOF

# On its own line, so a failure of harness_widths stops the harness (set -e)
# instead of running the loop zero times.
widths=$(harness_widths)
for width in $widths; do
    # shellcheck disable=SC2046 # harness_flags prints separate flags
    "$compiler" $(harness_flags "$width") -std=gnu11 -Wall -Wextra -Werror \
        -I"$work/sdl" -iquote port/include \
        scripts/port/test_xwayland_pointer.c port/src/port_xwayland_pointer.c -o "$work/test-$width"
    status=0
    output=$("$work/test-$width" 2>&1) || status=$?
    if [[ $status != 0 ]]; then
        echo "test_xwayland_pointer: ${width}-bit: exit $status; output:" >&2
        echo "$output" >&2
        exit 1
    fi
    echo "test_xwayland_pointer: PASS (${width}-bit): $output"
done
