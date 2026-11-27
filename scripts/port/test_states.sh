#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-state-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
test_bin=$work/test

# The 64-bit build links port_state_unavailable.c in place of port_state.c.
# One check per behaviour of the stand-in, each Arrange / Act / Assert.
cat >"$work/unavailable.c" <<'C'
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_state.h"
/* The game's own string.h and assert.h shadow the C library's: declare
 * strcmp, and make the game's assert, which only prints, stop the test. */
extern int strcmp(const char *, const char *);
extern char *strstr(const char *, const char *);
void osSyncPrintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    abort();
}
static const char expected[] = "Suspend points are unavailable in 64-bit builds. Campaign saves work as usual.";
static int inputs, prints;
static char printed[128];
/* joy_port.c: takes the suspend keys and runs the Graphics frame (test_state_input.c checks it) */
void portStateInput(void) { inputs++; }
void portConPrintf(s32 show, const char *fmt, ...)
{
    va_list args;
    (void)show;
    va_start(args, fmt);
    vsnprintf(printed, sizeof(printed), fmt, args);
    va_end(args);
    prints++;
}
static void infoReportsNoSlot(void)
{
    /* Arrange */
    PortStateInfo info;
    memset(&info, 0xff, sizeof(info));
    /* Act */
    s32 result = portStateInfo(PORT_STATE_QUICK, &info);
    /* Assert */
    assert(result == 0 && info.present == 0 && info.compatible == 0);
}
static void messageNamesTheWidth(void)
{
    /* Act */
    const char *message = portStateMessage();
    /* Assert */
    assert(strcmp(message, expected) == 0);
}
static void messageNamesCampaignSaves(void)
{
    /* Act */
    const char *message = portStateMessage();
    /* Assert */
    assert(strstr(message, "Campaign saves") != NULL);
}
static void rowLabelSaysUnavailable(void)
{
    /* Act */
    const char *label = portStateRowLabel();
    /* Assert */
    assert(strstr(label, "unavailable") != NULL);
}
static void menuOpenPrintsTheMessage(void)
{
    /* Arrange */
    prints = 0;
    printed[0] = '\0';
    /* Act */
    portStateMenuOpen();
    /* Assert */
    assert(prints == 1 && strcmp(printed, expected) == 0);
}
static void requestPrintsTheMessage(void)
{
    /* Arrange */
    prints = 0;
    printed[0] = '\0';
    /* Act */
    portStateRequest(PORT_STATE_QUICK, 1);
    /* Assert */
    assert(prints == 1 && strcmp(printed, expected) == 0);
}
static void frameTakesTheSuspendKeys(void)
{
    /* Arrange */
    inputs = 0;
    /* Act */
    portStateFrame();
    /* Assert */
    assert(inputs == 1);
}
static void neverPauses(void)
{
    /* Act and Assert */
    assert(portStatePaused() == 0 && portStateMenuInput(1) == 0);
}
int main(void)
{
    infoReportsNoSlot();
    messageNamesTheWidth();
    messageNamesCampaignSaves();
    rowLabelSaysUnavailable();
    menuOpenPrintsTheMessage();
    requestPrintsTheMessage();
    frameTakesTheSuspendKeys();
    neverPauses();
    return 0;
}
C

# compile WIDTH OUTPUT SOURCE... with the game's freestanding flags.
compile() {
    local width=$1 output=$2
    shift 2
    "${CC:-gcc}" $(harness_flags "$width" -msse2 -mfpmath=sse) -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -Werror=implicit-function-declaration \
        $(harness_gc_flags) \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        "$@" -lm -o "$output"
}

widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_states "$width"; then
        echo "test_states: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    # test_states.c includes port_state.c, which 64-bit builds do not compile;
    # test_state_input.c tests joy_port.c's routing with its own stubs.
    if [[ $width == 64 ]]; then
        echo "test_states: test_states.c (port_state.c's codec, menu and restore) is 32-bit only by design: suspend images record 32-bit addresses, so 64-bit builds link port_state_unavailable.c, checked here instead; test_state_input.c runs at both widths"
        compile "$width" "$test_bin" "$work/unavailable.c" port/src/port_state_unavailable.c
        # Like port_state.c, each notice is also a `ge: suspend:` log line: one for the menu, one
        # for the request.
        "$test_bin" 2>"$work/stderr" || { cat "$work/stderr" >&2; exit 1; }
        notices=0
        while IFS= read -r line; do
            if [[ $line == 'ge: suspend: Suspend points are unavailable in 64-bit builds. Campaign saves work as usual.' ]]; then
                notices=$((notices + 1))
            fi
        done <"$work/stderr"
        if [[ $notices != 2 ]]; then
            echo "test_states: expected 2 'ge: suspend:' log lines from the 64-bit stand-in, got $notices:" >&2
            cat "$work/stderr" >&2
            exit 1
        fi
    else
        compile "$width" "$test_bin" scripts/port/test_states.c
        "$test_bin"
    fi
    compile "$width" "$test_bin" scripts/port/test_state_input.c
    "$test_bin"
    echo "test_states: PASS (${width}-bit)"
done
python3 scripts/port/test_state_layout.py
