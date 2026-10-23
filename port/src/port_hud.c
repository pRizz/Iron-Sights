/* Optional modern HUD. Events come from accepted damage and objective changes;
 * presentation state is per player and advances in simulation time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <fr.h>
#include <str.h>
#include "game/bondview.h"
#include "game/bondinv.h"
#include "game/dyn.h"
#include "game/gun.h"
#include "game/front.h"
#include "game/lv.h"
#include "game/math_atan2f.h"
#include "game/options.h"
#include "game/player.h"
#include "game/textrelated.h"
#include "port_ads.h"
#include "port_hud.h"
#include "port_weapon_images.h"

extern s32 portBondControlsLive(void);
extern s32 bondinvGetTextbyInvIndex(s32 index);
extern u16 *bondinvGetNameByIndex(s32 index);
/* The decomp's minimal string.h omits these CRT declarations. */
extern void *memset(void *dst, int value, size_t size);
extern void *memmove(void *dst, const void *src, size_t size);

#define HUD_TOASTS 8
#define HUD_WEAPONS 32
#define HUD_PI 3.14159265358979323846f
static const char *const phKeys[] = { "hit_markers", "damage_indicators", "objective_notifications", "weapon_hud", "weapon_hud_icons", "always_health" };
static s32 phOptions[] = { 1, 1, 1, 1, 0, 1 };
#define HUD_OPTIONS (sizeof(phOptions) / sizeof(phOptions[0]))
typedef struct { f32 time, bearing; s32 directional; } HudDamage;
typedef struct { s32 index, letter, status; char text[192]; } HudToast;
typedef struct {
    f32 hitTime, toastTime;
    f32 weaponIdle, healthIdle, health, armour;
    s32 observed, weapon;
    s32 headshot, toastCount;
    HudDamage damage[4];
    HudToast toasts[HUD_TOASTS];
} HudState;
static HudState phState[4];

s32 portHudMissionTargetSeconds(s32 stage, s32 difficulty)
{
    s32 i;
    if (difficulty < DIFFICULTY_AGENT || difficulty > DIFFICULTY_00) return 0;
    /* Stage IDs and mission indices differ. Resolve the live stage, not the
     * currently highlighted folder/briefing row (also valid after a restore). */
    for (i = 0; mission_folder_setup_entries[i].folder_text_preset; i++) {
        const struct mission_folder_setup *entry = &mission_folder_setup_entries[i];
        if (entry->stage_id == stage && entry->mission_num >= 0 && entry->mission_num < 20)
            return solo_target_time_array[entry->mission_num][difficulty];
    }
    return 0;
}

void portHudMissionTimerText(s32 ticks, s32 target, char *text, s32 size)
{
    if (size <= 0) return;
    if (ticks < 0) ticks = 0;
    if (target > 0)
        snprintf(text, size, "%02d:%02d:%02d / CHEAT %02d:%02d", ticks/3600,
            (ticks/60)%60, ((ticks%60)*100)/60, target/60, target%60);
    else
        snprintf(text, size, "%02d:%02d:%02d", ticks/3600,
            (ticks/60)%60, ((ticks%60)*100)/60);
}

u32 portHudMissionTargetColor(s32 ticks, s32 target)
{
    const s32 seconds = ticks > 0 ? ticks/60 : 0;
    /* Match file.c's whole-second <= comparison, including the full final
     * second. Color describes time only; objectives/cheat rules still apply. */
    if (target <= 0) return 0xffffffff;
    if (seconds > target) return 0xff9d91ff;
    if (target-seconds <= 10) return 0xf0d994ff;
    return 0xb9ed9fff;
}

s32 portHudOptionGet(const char *key, s32 *value)
{
    s32 i;
    for (i = 0; i < HUD_OPTIONS; i++) if (strcmp(key, phKeys[i]) == 0) { *value = phOptions[i]; return 1; }
    return 0;
}

s32 portHudOptionSet(const char *key, s32 value)
{
    s32 i, p;
    for (i = 0; i < HUD_OPTIONS; i++) if (strcmp(key, phKeys[i]) == 0) {
        phOptions[i] = value != 0;
        for (p = 0; p < 4; p++) {
            if (i == 3 || i == 4) phState[p].weaponIdle = 0;
            if (i == 5) phState[p].healthIdle = 0;
        }
        if (!value) for (p = 0; p < 4; p++) {
            if (i == 0) phState[p].hitTime = 0;
            if (i == 1) memset(phState[p].damage, 0, sizeof(phState[p].damage));
            if (i == 2) phState[p].toastCount = 0;
        }
        return 1;
    }
    return 0;
}

void portHudReset(s32 player)
{
    if (player >= 0 && player < 4) memset(&phState[player], 0, sizeof(phState[player]));
}

void portHudHit(s32 attacker, s32 headshot)
{
    if (!phOptions[0] || attacker < 0 || attacker >= 4) return;
    /* A body pellet must not overwrite a headshot from the same blast. */
    phState[attacker].headshot = headshot || (phState[attacker].hitTime > 10.0f && phState[attacker].headshot);
    phState[attacker].hitTime = 12.0f;
}

void portHudDamage(s32 victim, f32 incomingX, f32 incomingZ)
{
    s32 i, slot = 0;
    HudState *h;
    if (victim < 0 || victim >= 4) return;
    h = &phState[victim];
    h->healthIdle = 0; /* Gauge emphasis is independent of damage indicators. */
    if (!phOptions[1]) return;
    for (i = 1; i < 4; i++) if (h->damage[i].time < h->damage[slot].time) slot = i;
    h->damage[slot].time = 60.0f;
    h->damage[slot].directional = incomingX * incomingX + incomingZ * incomingZ > 0.0001f;
    /* The incoming vector points away from the attacker. Store world bearing
     * so the indicator follows the source when the victim turns. */
    h->damage[slot].bearing = atan2f(-incomingX, -incomingZ);
}

static void phCopyLine(char *out, s32 size, const char *text)
{
    s32 n = 0;
    if (text) while (*text && n < size - 1) {
        char c = *text++;
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        out[n++] = c;
    }
    while (n && out[n-1] == ' ') n--;
    out[n] = '\0';
}

s32 portHudObjective(s32 index, s32 letter, s32 status, const char *text)
{
    HudState *h = &phState[0];
    HudToast *t;
    s32 i;
    if (!phOptions[2] || getPlayerCount() != 1) return 0;
    for (i = 0; i < h->toastCount; i++) if (h->toasts[i].index == index && h->toasts[i].status == status) return 1;
    if (h->toastCount == HUD_TOASTS) {
        /* Preserve the visible notification; discard the oldest pending one. */
        memmove(&h->toasts[1], &h->toasts[2], sizeof(HudToast) * (HUD_TOASTS - 2));
        h->toastCount--;
    }
    if (!h->toastCount) h->toastTime = 270.0f;
    t = &h->toasts[h->toastCount++];
    t->index = index; t->letter = letter; t->status = status;
    phCopyLine(t->text, sizeof(t->text), text);
    return 1;
}

static void phAdvance(HudState *h, f32 dt)
{
    s32 i;
    if (dt <= 0) return;
    h->weaponIdle = h->weaponIdle + dt < 240 ? h->weaponIdle + dt : 240;
    h->healthIdle = h->healthIdle + dt < 240 ? h->healthIdle + dt : 240;
    h->hitTime = h->hitTime > dt ? h->hitTime - dt : 0;
    for (i = 0; i < 4; i++) h->damage[i].time = h->damage[i].time > dt ? h->damage[i].time - dt : 0;
    while (h->toastCount && dt > 0) {
        if (dt < h->toastTime) { h->toastTime -= dt; break; }
        dt -= h->toastTime;
        memmove(&h->toasts[0], &h->toasts[1], sizeof(HudToast) * --h->toastCount);
        h->toastTime = 270.0f;
    }
}

static void phObserve(HudState *h, s32 weapon, f32 health, f32 armour)
{
    if (!h->observed || h->weapon != weapon) h->weaponIdle = 0;
    if (!h->observed || h->health != health || h->armour != armour) h->healthIdle = 0;
    h->weapon = weapon; h->health = health; h->armour = armour; h->observed = 1;
}

/* Hold full emphasis for three seconds, then ease to the idle style over one. */
static f32 phQuietScale(f32 idle, f32 quiet)
{
    f32 t = (idle-180.0f)/60.0f;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    t = t*t*(3.0f-2.0f*t);
    return 1.0f + (quiet-1.0f)*t;
}

static u32 phOpacity(u32 rgba, f32 opacity)
{
    return (rgba & 0xffffff00u) | (u32)((rgba & 255)*opacity+0.5f);
}

void portHudTick(void)
{
    const s32 p = get_cur_playernum();
    if (p < 0 || p >= 4 || !portBondControlsLive() || !g_CurrentPlayer) return;
    phAdvance(&phState[p], g_GlobalTimerDelta);
    phObserve(&phState[p], getCurrentPlayerWeaponId(GUNRIGHT),
        g_CurrentPlayer->bondhealth, g_CurrentPlayer->bondarmour);
#ifdef GE_VIDEO
    { /* Isolated visual audit: no input or gameplay changes without this knob. */
        static f32 time;
        static s32 fired;
        static s32 equipped;
        static s32 switched;
        const char *audit = getenv("GE_HUD_AUDIT");
        if (p == 0 && audit && fired < 4) {
            if (!equipped && atoi(audit) >= ITEM_WPPK && atoi(audit) <= ITEM_TASER
                && atoi(audit) != ITEM_TRIGGER) {
                currentPlayerEquipWeaponWrapper(GUNLEFT, ITEM_UNARMED);
                currentPlayerEquipWeaponWrapper(GUNRIGHT, atoi(audit));
                equipped = 1;
            }
            time += g_GlobalTimerDelta;
            if (!fired && time >= 100.0f) {
                extern void portConLogf(const char *fmt, ...);
                portHudHit(0, getenv("GE_HUD_AUDIT_BODY") == NULL);
                g_CurrentPlayer->bondhealth = 0.625f;
                g_CurrentPlayer->bondarmour = 0.5f;
                /* Known attacker on Bond's right, using the game's movement
                 * basis: right = (-cos(heading), -sin(heading)). */
                const f32 heading = g_CurrentPlayer->vv_theta * HUD_PI / 180.0f;
                portHudDamage(0, cosf(heading), sinf(heading));
                portHudObjective(0, 0, OBJECTIVESTATUS_COMPLETE, "Neutralise all alarms");
                portHudObjective(1, 1, OBJECTIVESTATUS_FAILED, "Install covert modem");
                fired = 1;
                portConLogf("ge: hud-audit: hit, direction, completed objective, weapon strip\n");
            } else if (fired == 1 && time >= 104.0f) {
                extern int ge_shot_force;
                ge_shot_force = 1;
                fired = 2;
            } else if (fired == 2 && time >= 380.0f) {
                extern int ge_shot_force;
                ge_shot_force = 1;
                fired = 3;
            } else if (fired == 3 && !switched && time >= 650.0f) {
                currentPlayerEquipWeaponWrapper(GUNRIGHT, atoi(audit) == ITEM_WPPK ? ITEM_AK47 : ITEM_WPPK);
                switched = 1;
            } else if (fired == 3 && time >= 670.0f) {
                extern int ge_shot_force;
                ge_shot_force = 1;
                fired = 4;
            }
        }
    }
#endif
}

/* Keep the strip independent of Ammo On Screen and its damage blink. */
s32 portHudWeaponStripShown(void)
{
    return phOptions[3] && portBondControlsLive() && g_CurrentPlayer
        && !g_CurrentPlayer->mpmenuon && !g_CurrentPlayer->hudmessoff;
}

s32 portHudWeaponStripTopInset(void)
{
    /* Reserve a small caption row above the original ammo counters. Keep the
     * icon row stable when the caption fades or a gadget is equipped. */
    return PORT_HUD_WEAPON_BOTTOM + (phOptions[4] && PORT_WEAPON_IMAGES_AVAILABLE ? 7 : 0);
}

s32 portHudAlwaysHealthShown(void)
{
    return phOptions[5] && portBondControlsLive() && g_CurrentPlayer
        && !g_CurrentPlayer->bonddead && !g_CurrentPlayer->mpmenuon && !g_CurrentPlayer->hudmessoff;
}

void portHudStyleGauge(struct damage_display_val *vertices, s32 count)
{
    const s32 p = get_cur_playernum();
    f32 thickness, opacity;
    s32 i;
    if (!portHudAlwaysHealthShown() || p < 0 || p >= 4) return;
    /* Slimmer even when emphasized; quiet gauges retain only a fine trace. */
    thickness = phQuietScale(phState[p].healthIdle, 0.2f / 0.75f) * 0.75f;
    opacity = phQuietScale(phState[p].healthIdle, 0.25f);
    for (i = 0; i+1 < count; i += 2) {
        struct damage_display_val *a = &vertices[i], *b = &vertices[i+1];
        const f32 cx = (a->pos.x+b->pos.x)*0.5f, cz = (a->pos.z+b->pos.z)*0.5f;
        a->pos.x = (s16)(cx+(a->pos.x-cx)*thickness);
        b->pos.x = (s16)(cx+(b->pos.x-cx)*thickness);
        a->pos.z = (s16)(cz+(a->pos.z-cz)*thickness);
        b->pos.z = (s16)(cz+(b->pos.z-cz)*thickness);
        a->colour.a = (u8)(a->colour.a*opacity+0.5f);
        b->colour.a = (u8)(b->colour.a*opacity+0.5f);
    }
}

typedef struct { s32 item, inventory; } HudWeapon;
static const char *phWeaponName(const HudWeapon *weapon)
{
    const char *name = (const char *)bondinvGetNameByIndex(weapon->inventory);
    if (!name || !name[0]) name = (const char *)get_ptr_short_watch_text_for_item(weapon->item);
    return name && name[0] ? name : "Unarmed";
}

static s32 phWeaponIcon(s32 item)
{
    return item >= 0 && item < 32 && portWeaponImages[item].size ? item : -1;
}

static s32 phWeapons(HudWeapon *weapons, s32 *selected)
{
    const s32 total = bondinvCountTotalItemsInInv();
    const s32 current = getCurrentPlayerWeaponId(GUNRIGHT);
    s32 i, j, n = 0;
    *selected = -1;
    for (i = 0; i < total && n < HUD_WEAPONS; i++) {
        const s32 item = bondinvGetTextbyInvIndex(i);
        if (item < ITEM_UNARMED || item > ITEM_TASER || item == ITEM_WATCHLASER || item == ITEM_TRIGGER) continue;
        for (j = 0; j < n && weapons[j].item != item; j++) {}
        if (j < n) continue;
        weapons[n].item = item; weapons[n].inventory = i;
        if (item == current) *selected = n;
        n++;
    }
    return n;
}

static s32 phStripStart(s32 count, s32 selected, s32 slots)
{
    s32 start = selected < 0 ? 0 : selected - slots / 2;
    if (start > count - slots) start = count - slots;
    return start < 0 ? 0 : start;
}

static f32 phDamageAngle(const HudDamage *damage, f32 heading)
{
    /* Bond's screen-right basis is (-cos(theta), -sin(theta)). Reverse the
     * world bearing's handedness for screen space; cosine keeps front/back. */
    return -damage->bearing - heading * HUD_PI / 180.0f;
}

static Gfx *phVector(Gfx *g, s32 shape, f32 x, f32 y, f32 sx, f32 sy, f32 start, f32 end, u32 rgba)
{
    const f32 values[] = {x, y, sx, sy, start, end};
    s32 i;
    if (dynGetFreeGfx2(g) < 516) return g;
    g->words.w0 = ((u32)0x4e << 24) | shape; /* G_HUD_VECTOR_EXT */
    g->words.w1 = rgba;
    g++;
    for (i = 0; i < 6; i += 2) {
        memcpy(&g->words.w0, &values[i], sizeof(f32));
        memcpy(&g->words.w1, &values[i+1], sizeof(f32));
        g++;
    }
    return g;
}

Gfx *portHudReticle(Gfx *g, f32 x, f32 y, f32 scaleX, f32 scaleY, u32 color, s32 ring)
{
    if (scaleX <= 0 || scaleY <= 0) return g;
    return phVector(g, ring ? 2 : 1, x, y, scaleX, scaleY, 0, 0, color);
}

/* Quarter-pixel fills permit translucent rectangles without textures. */
static Gfx *phRect(Gfx *g, f32 x, f32 y, f32 w, f32 h, u32 rgba)
{
    s32 x0 = (s32)(x * 4), y0 = (s32)(y * 4);
    s32 x1 = (s32)((x + w) * 4), y1 = (s32)((y + h) * 4);
    if (dynGetFreeGfx2(g) < 520) return g;
    gDPPipeSync(g++);
    gDPSetCycleType(g++, G_CYC_1CYCLE);
    gDPSetRenderMode(g++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(g++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(g++, 0, 0, rgba >> 24, rgba >> 16, rgba >> 8, rgba);
    g->words.w0 = ((u32)G_FILLRECT << 24) | ((x1 & 0xfff) << 12) | (y1 & 0xfff);
    g->words.w1 = ((x0 & 0xfff) << 12) | (y0 & 0xfff);
    return g + 1;
}

static Gfx *phTextAligned(Gfx *g, const char *text, s32 x, s32 y, f32 scale, u32 color, s32 maxWidth, s32 centered)
{
    char line[196];
    s32 h, w;
    if (!ptrFontBankGothicChars || !ptrFontBankGothic || maxWidth <= 0) return g;
    phCopyLine(line, sizeof(line)-2, text);
    portTextScale(scale);
    for (;;) {
        textMeasure(&h, &w, line, ptrFontBankGothicChars, ptrFontBankGothic, 0);
        if (w <= maxWidth || !line[0]) break;
        line[strlen(line)-1] = '\0';
    }
    if (centered) {
        const s32 left = viGetViewLeft()+4, right = viGetViewLeft()+viGetViewWidth()-4;
        x -= w/2;
        if (x+w > right) x = right-w;
        if (x < left) x = left;
    }
    strcat(line, "\n");
    if (dynGetFreeGfx2(g) < 540 + (s32)strlen(line)*12) { portTextScale(1); return g; }
    g = microcode_constructor(g);
    /* Only plain textRender shares portTextScale with textMeasure. */
    g = textRender(g, &x, &y, line, ptrFontBankGothicChars, ptrFontBankGothic,
        color, viGetX(), viGetY(), 0, 0);
    portTextScale(1.0f);
    return g;
}

static Gfx *phText(Gfx *g, const char *text, s32 x, s32 y, f32 scale, u32 color, s32 maxWidth)
{
    return phTextAligned(g, text, x, y, scale, color, maxWidth, FALSE);
}

static Gfx *phDrawStrip(Gfx *g)
{
    HudWeapon weapons[HUD_WEAPONS];
    s32 selected, n = phWeapons(weapons, &selected);
    const f32 scale = 0.7f, pitch = 54 * scale;
    s32 width = viGetViewWidth(), slots = (s32)((width - 36 * scale) / pitch);
    s32 i, start;
    f32 x, y = viGetViewTop() + viGetViewHeight() - portHudWeaponStripTopInset();
    const f32 opacity = phQuietScale(phState[get_cur_playernum()].weaponIdle, 0.2f);
    const f32 captionOpacity = phQuietScale(phState[get_cur_playernum()].weaponIdle, 0.0f);
    char label[64];
    if (!n || slots < 1) return g;
    if (slots > 5) slots = 5;
    if (slots > n) slots = n;
    start = phStripStart(n, selected, slots);
    x = viGetViewLeft() + (width - slots * pitch) / 2;
    for (i = 0; i < slots; i++) {
        const s32 active = start + i == selected;
        const s32 icon = phOptions[4] ? phWeaponIcon(weapons[start+i].item) : -1;
        g = phRect(g, x + i*pitch, y, 52*scale, 17*scale, phOpacity(active ? 0x53683cce : 0x10171cb0, opacity));
        if (active) g = phRect(g, x + i*pitch, y+15*scale, 52*scale, 2*scale, phOpacity(0xd7f3a9ff, opacity));
        if (icon >= 0) {
            /* Native 512x160 RGBA model render, with its original colours. */
            g = phVector(g, 4, x+i*pitch+2*scale, y+scale, 48*scale, 14*scale, icon, 0, phOpacity(0xffffffff, opacity));
        } else {
            g = phText(g, phWeaponName(&weapons[start+i]), (s32)(x + i*pitch + 3*scale + 0.5f), (s32)(y+4*scale+0.5f), 0.65f*scale,
                phOpacity(active ? 0xf2ffdcff : 0xc3ccd4ff, opacity), (s32)(46*scale));
        }
    }
    if (start) {
        snprintf(label, sizeof(label), "+%d", start);
        g = phText(g, label, (s32)(x-17*scale+0.5f), (s32)(y+5*scale+0.5f), 0.6f*scale, phOpacity(0xc3ccd4ff, opacity), (s32)(17*scale));
    }
    if (start+slots < n) {
        snprintf(label, sizeof(label), "+%d", n-start-slots);
        g = phText(g, label, (s32)(x+slots*pitch+2*scale+0.5f), (s32)(y+5*scale+0.5f), 0.6f*scale, phOpacity(0xc3ccd4ff, opacity), (s32)(18*scale));
    }
    if (phOptions[4] && selected >= 0 && phWeaponIcon(weapons[selected].item) >= 0 && captionOpacity > 0) {
        const s32 center = (s32)(x+(selected-start)*pitch+26*scale+0.5f);
        const s32 top = (s32)(y+17*scale+1.5f);
        const s32 maxWidth = width < 108 ? width-12 : 96;
        const char *name = phWeaponName(&weapons[selected]);
        g = phTextAligned(g, name, center+1, top+1, 0.5f,
            phOpacity(0x000000dd, captionOpacity), maxWidth, TRUE);
        g = phTextAligned(g, name, center, top, 0.5f,
            phOpacity(0xf2ffdcff, captionOpacity), maxWidth, TRUE);
    }
    return g;
}

Gfx *portHudPanel(Gfx *g, f32 x, f32 y, f32 w, f32 h, u32 color)
{
    return phRect(g, x, y, w, h, color);
}

Gfx *portHudLabel(Gfx *g, const char *text, s32 x, s32 y, f32 scale, u32 color, s32 maxWidth)
{
    return phText(g, text, x, y, scale, color, maxWidth);
}

Gfx *portHudDraw(Gfx *g)
{
    const s32 p = get_cur_playernum();
    HudState *h;
    s32 i;
    f32 cx, cy;
    if (p < 0 || p >= 4 || !portBondControlsLive() || !g_CurrentPlayer
        || g_CurrentPlayer->hudmessoff || g_CurrentPlayer->mpmenuon
        || !ptrFontBankGothic || !ptrFontBankGothicChars) return g;
    h = &phState[p];
    cx = viGetViewLeft() + viGetViewWidth() * 0.5f;
    cy = viGetViewTop() + viGetViewHeight() * 0.5f;
#ifdef GE_VIDEO
    if (p == 0 && h->hitTime > 0 && getenv("GE_HUD_AUDIT")) {
        static s32 logged;
        if (!logged) {
            extern void portConLogf(const char *fmt, ...);
            portConLogf("ge: hud-audit: draw hit=%.1f damage=%.1f center=%.1f,%.1f aim=%.1f,%.1f free=%d\n",
                h->hitTime, h->damage[0].time, cx, cy,
                g_CurrentPlayer->crosshair_angle.x, g_CurrentPlayer->crosshair_angle.y, dynGetFreeGfx2(g));
            logged = 1;
        }
    }
#endif
    if (phOptions[3]) g = phDrawStrip(g);
    if (phOptions[2] && h->toastCount && getPlayerCount() == 1) {
        const HudToast *t = &h->toasts[0];
        const s32 complete = t->status == OBJECTIVESTATUS_COMPLETE;
        const s32 failed = t->status == OBJECTIVESTATUS_FAILED;
        const u32 color = complete ? 0xb9ed9fff : failed ? 0xff9d91ff : 0xf0d994ff;
        const s32 w = viGetViewWidth() - 48, x = viGetViewLeft()+24, y = viGetViewTop()+28;
        char title[64];
        snprintf(title, sizeof(title), "OBJECTIVE %c %s", 'A'+t->letter,
            complete ? "COMPLETED" : failed ? "FAILED" : "UPDATED");
        g = phRect(g, x, y, w, 30, 0x10171cda);
        g = phRect(g, x, y, 2, 30, color);
        g = phText(g, title, x+8, y+4, 0.8f, color, w-16);
        g = phText(g, t->text, x+8, y+17, 0.75f, 0xe1e5eaff, w-16);
    }
    for (i = 0; phOptions[1] && i < 4; i++) if (h->damage[i].time > 0) {
        const HudDamage *d = &h->damage[i];
        const f32 a = phDamageAngle(d, g_CurrentPlayer->vv_theta);
        const u32 color = 0xff514400 | (u32)((d->time < 20 ? d->time / 20 : 1) * 220);
        if (!d->directional) {
            g = phRect(g, viGetViewLeft()+7, cy-14, 2, 28, color);
            g = phRect(g, viGetViewLeft()+viGetViewWidth()-9, cy-14, 2, 28, color);
        } else {
            /* Shared curve renderer: preserve the direction and ellipse. */
            g = phVector(g, 3, cx, cy, 47, 34, a-HUD_PI*0.5f-0.26f, a-HUD_PI*0.5f+0.26f, color);
        }
    }
    if (phOptions[0] && h->hitTime > 0) {
        f32 x = g_CurrentPlayer->crosshair_angle.x, y = g_CurrentPlayer->crosshair_angle.y;
        const u32 color = (h->headshot ? 0xf6cf7900 : 0xf1f5f900) | (u32)((h->hitTime < 5 ? h->hitTime/5 : 1) * 255);
        if (portAdsActive()) { x = cx; y = cy; }
        if (dynGetFreeGfx2(g) >= 514) {
            /* G_HITMARKER_EXT (gbiex_compat.h): the PC renderer draws continuous
             * feathered lines, preserving fractional aim positions at any scale. */
            g->words.w0 = (u32)0x4d << 24;
            g->words.w1 = color;
            g++;
            memcpy(&g->words.w0, &x, sizeof(x));
            memcpy(&g->words.w1, &y, sizeof(y));
            g++;
        }
    }
    return combiner_bayer_lod_perspective(g);
}
