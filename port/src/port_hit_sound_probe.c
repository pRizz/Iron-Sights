/* GE_HITSOUND_PROBE=<metres,...> (test harness, FORK-043): in a solo stage,
 * replay a confirmed hit at each distance twice, Hit Sounds off then on. Each
 * hit runs the native impact (gunfire.c) and then portHitSoundConfirmed, as a
 * player's bullet does (chr.c), from a stand-in character overhead so the pan stays
 * centred. Pair it with GE_AUDIO_CAPTURE; the log gives each hit's frame. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/gun.h"
#include "game/propobj.h"
#include "port_audio_capture.h"
#include "port_hit_sound.h"

#define PROBE_MAX      16
#define PROBE_SPACING  60 /* ticks between hits: a second at 60 Hz */
#define PROBE_UNITS    100.0f /* world units per metre */

extern PropRecord *getCurrentPlayerProp(void);
void portHitSoundProbeTick(void);

static f32 probeMetres[PROBE_MAX];
static s32 probeCount = -1; /* -1 unread, 0 off */
static s32 probeTick;
static s32 probeNext;
static s32 probeRestore = 1;

static void probeRead(void)
{
    const char *e = getenv("GE_HITSOUND_PROBE");
    const char *after = getenv("GE_HITSOUND_PROBE_AFTER");

    probeCount = 0;
    if (e == NULL || e[0] == '\0') return;
    while (*e != '\0' && probeCount < PROBE_MAX) {
        char *end;
        const f32 metres = (f32)strtod(e, &end);

        if (end == e) break;
        probeMetres[probeCount++] = metres;
        e = (*end == ',') ? end + 1 : end;
    }
    probeTick = -((after != NULL) ? atoi(after) : 240);
    portHitSoundOptionGet("hit_sounds", &probeRestore);
}

static void probeFire(PropRecord *bond, s32 index)
{
    static PropRecord target;
    const s32 enabled = index & 1;
    const f32 metres = probeMetres[index / 2];
    s32 native;

    memset(&target, 0, sizeof(target));
    target.type = PROP_TYPE_CHR;
    target.pos = bond->pos;
    target.pos.y += metres * PROBE_UNITS;
    native = sub_GAME_7F0539E4(&target.pos);
    portHitSoundOptionSet("hit_sounds", enabled);
    fprintf(stderr, "ge: hitprobe hit=%d metres=%.1f hit_sounds=%d native=%d frame=%u\n",
            index, metres, enabled, native, portAudioCaptureFrames());
    recall_joy2_hits_edit_detail_edit_flag(ITEM_AK47, &target, -1);
    portHitSoundConfirmed(&target);
}

void portHitSoundProbeTick(void)
{
    PropRecord *bond;

    if (probeCount < 0) probeRead();
    if (probeCount == 0 || probeNext >= probeCount * 2) return;
    bond = getCurrentPlayerProp();
    if (bond == NULL || bond->stan == NULL) return; /* not in a mission yet */
    if (probeTick++ < 0 || probeTick % PROBE_SPACING != 0) return;
    probeFire(bond, probeNext++);
    if (probeNext == probeCount * 2) {
        portHitSoundOptionSet("hit_sounds", probeRestore);
        fprintf(stderr, "ge: hitprobe done\n");
    }
}
