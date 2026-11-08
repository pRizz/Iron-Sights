#!/usr/bin/env bash
# LeakSanitizer renderer run (scripts/port/buffer_audit.md, "Leak detection").
# Runs a GE_VIDEO=ON + GE_SANITIZE=ON i686 executable under Xvfb with Mesa's
# software renderer, one isolated runtime folder per case, and fails when any
# case reports a leak, an ASan/UBSan error or a nonzero exit.
#
# Inside midas-linux-release, with the worktree at /src and the ROM read-only:
#   bash scripts/port/leak_audit_video.sh <exe> <log-dir> 'name|frames|VAR=value ...' ...
# The executable's folder must hold lib/libSDL2-2.0.so.0 and Data/.
set -uo pipefail
exe=$(realpath "$1"); logs=$(realpath -m "$2"); shift 2
rom=${GE_ROM:-/rom/baserom.u.z64}
suppressions=$(realpath "$(dirname "$0")/lsan_suppressions.txt")
mkdir -p "$logs"
if ! command -v xvfb-run >/dev/null; then
    dpkg --add-architecture i386 && apt-get update -qq && apt-get install -y -qq xvfb xauth \
        libgl1:i386 libgl1-mesa-dri:i386 libegl1:i386 libx11-6:i386 libxext6:i386 libxcursor1:i386 \
        libxi6:i386 libxrandr2:i386 libxinerama1:i386 libxxf86vm1:i386 libxss1:i386 libasound2:i386 \
        libcurl4:i386 >/dev/null || exit 1
fi
status=0
for spec in "$@"; do
    IFS='|' read -r name frames vars <<< "$spec"
    work=$(mktemp -d /tmp/midas-leak.XXXXXX)
    cp "$exe" "$work/"
    cp -r "$(dirname "$exe")/lib" "$(dirname "$exe")/Data" "$work/"
    cp "$(dirname "$exe")"/*.txt "$work/" 2>/dev/null
    (
        cd "$work" || exit 1
        for assignment in $vars; do export "$assignment"; done
        # Small window, no MSAA/mipmaps: Mesa's softpipe stays usable under ASan.
        printf '[display]\nwindow_width=640\nwindow_height=480\nfullscreen=windowed\nmaxfps=60\n' > midas.ini
        printf 'render_thread=%s\nmsaa=1\nfiltering=point\n' "${RT:-0}" >> midas.ini
        export GE_ROM="$rom" GE_FRAMES="$frames" GE_SEED=1 GE_NOINPUT=1 GE_HWMIPMAPS=0
        export GE_ICON_CACHE_DIR="$work/cache" LD_LIBRARY_PATH="$work/lib"
        export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=softpipe SDL_AUDIODRIVER=dummy
        export ASAN_OPTIONS=detect_leaks=1
        export LSAN_OPTIONS="suppressions=$suppressions:print_suppressions=1"
        timeout "${RUN_TIMEOUT:-1800}" xvfb-run -a -s '-screen 0 1280x800x24' "./$(basename "$exe")" \
            > "$logs/$name.log" 2>&1
        echo "exit=$?" >> "$logs/$name.log"
    )
    rm -rf "$work"
    log="$logs/$name.log"
    if tail -1 "$log" | grep -qx 'exit=0' && grep -q "frame limit reached: frames=$frames" "$log" \
        && ! grep -q 'ERROR: LeakSanitizer\|ERROR: AddressSanitizer\|runtime error:' "$log"; then
        echo "$name: PASS"
    else
        echo "$name: FAIL ($log)"; status=1
    fi
done
exit $status
