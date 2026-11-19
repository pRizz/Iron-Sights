#!/usr/bin/env bash
# Builds test_audio_synth.c with the synthesizer's sources and the port's
# interpreter at each pointer width the host can build and run, and runs it
# (LP64-005 Task 10). The bank it plays is built in the test from invented
# numbers; no ROM is read.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-audio-synth.XXXXXX)
trap 'rm -rf "$work"' EXIT

# The synthesizer's sources as CMakeLists.txt lists them for the game, less
# the sequencers and the sound player, which the test does not drive.
synth_sources=(
    src/libultra/audio/sl.c src/libultra/audio/synthesizer.c src/libultra/audio/heapinit.c
    src/libultra/audio/heapalloc.c src/libultra/audio/copy.c src/libultra/audio/synaddplayer.c
    src/libultra/audio/synallocfx.c src/libultra/audio/synallocvoice.c src/libultra/audio/syndelete.c src/libultra/audio/synfreevoice.c
    src/libultra/audio/synsetvol.c src/libultra/audio/synstartvoiceparam.c src/libultra/audio/synstopvoice.c
    src/libultra/audio/load.c src/libultra/audio/save.c src/libultra/audio/resample.c
    src/libultra/audio/filter.c src/libultra/audio/mainbus.c src/libultra/audio/auxbus.c
    src/libultrare/audio/drvrNew.c src/libultrare/audio/reverb.c src/libultrare/audio/env.c
)
for width in $(harness_widths); do
    flags=($(harness_flags "$width" -msse2 -mfpmath=sse) -g -O1)
    "${CC:-gcc}" "${flags[@]}" -std=gnu11 -fms-extensions -fcommon -w -fno-builtin -fwrapv -funsigned-char \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0 -DNONMATCHING -DAVOID_UB \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include -Isrc/libultra \
        scripts/port/test_audio_synth.c "${synth_sources[@]}" -lm -o "$work/test_audio_synth_$width"
    "$work/test_audio_synth_$width"
    echo "test_audio_synth: PASS (${width}-bit)"
done
