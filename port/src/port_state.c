/* Compatible-build suspend images of registered engine memory. Host resources,
 * stacks, file handles, profile progress and platform input are never copied. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
extern void *memset(void *, int, size_t);
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "boss.h"
#include "game/player.h"
#include "game/bondview.h"
#include "game/front.h"
#include "game/file.h"
#include "game/lv.h"
#include "game/gun.h"
#include "game/chr.h"
#include "romdata.h"
#include "port_state.h"
#include "port_hud.h"
#include "port_mission_hints.h"
#include "fr.h"
extern void *memset(void *, int, size_t);
extern int memcmp(const void *, const void *, size_t);
extern int strcmp(const char *, const char *);
extern void *portGetMempArena(u32 *size);
extern void *portStateHighArena(u32 *used);
extern void portStateHighWater(u32 used);
extern const char *portRootPath(const char *name);
extern void portConPrintf(s32 show, const char *fmt, ...);
extern void portBindCooldownStart(void);
extern void portStatePresentationReset(void);
extern s32 portBondControlsLive(void);
extern s32 getMissiontimer(void);

#define STATE_VERSION 1
#define STATE_MAX_REGIONS 4096
#define STATE_MAX_BYTES (192u*1024u*1024u)
typedef struct { u32 addr, size; } StateRegion;
typedef struct {
    char magic[8];
    u32 version, headerSize, folder, stage, difficulty, timestamp, seconds;
    f32 health, armour;
    u32 count, mainBase, mainSize, highBase, highSize, romBase, romSize, romCRC;
    u8 build[32];
    u32 checksum;
} StateHeader;
typedef struct { StateHeader h; u8 **data; } StateImage;
static StateRegion stateRegions[STATE_MAX_REGIONS];
static u32 stateCount;
static u8 stateBuild[32];
static s32 stateLayout;
static s32 stateCommand = -1, stateLoad, stateWaiting;
static s32 stateSourceSlot;
static StateImage statePending;
static char stateMessage[192];
static char stateToast[128];
static OSTime stateToastTime;
static u32 stateRestoreSerial;
static s32 stateOpen, stateRow, stateChoice, stateConfirm, statePause;
static s32 stateResumePending;
static const char *stateStageName(u32 stage);

static u32 stateCRC(const void *ptr, u32 bytes)
{
    static u32 table[256];
    const u8 *p = ptr; u32 c = ~0u, i, bit;
    if (!table[1]) for (i = 0; i < 256; i++) {
        u32 v = i;
        for (bit = 0; bit < 8; bit++) v = (v>>1) ^ (0xedb88320u & (0u-(v&1)));
        table[i] = v;
    }
    for (i = 0; i < bytes; i++) c = table[(c^p[i])&255]^(c>>8);
    return ~c;
}

static void stateMessageSet(const char *text, s32 popup)
{
    snprintf(stateMessage, sizeof(stateMessage), "%s", text);
    fprintf(stderr,"ge: suspend: %s\n",text);
    portConPrintf(popup, "%s", text);
}

static void stateSay(const char *text) { stateMessageSet(text,1); }

static void stateNotify(s32 slot, s32 load, u32 stage)
{
    const char *mission=stateStageName(stage);
    if (slot==PORT_STATE_UNDO) snprintf(stateToast,sizeof(stateToast),"Undo load restored - %s",mission);
    else if (slot==PORT_STATE_QUICK) snprintf(stateToast,sizeof(stateToast),load ? "Loaded quicksave - %s" : "Quicksaved - %s",mission);
    else snprintf(stateToast,sizeof(stateToast),load ? "Loaded slot %d - %s" : "Saved slot %d - %s",slot+1,mission);
    stateToastTime=osGetTime();
    stateMessageSet(stateToast,0); /* The dedicated toast is the visible success message. */
}

/* Real time keeps notification duration independent of simulation rate and
 * makes it fade even while the slot menu is open. New actions replace old ones. */
static u32 stateToastAlpha(OSTime now)
{
    const OSTime duration=3ull*46875000ull, fade=46875000ull/2;
    const OSTime elapsed=now-stateToastTime;
    if (!stateToast[0] || elapsed>=duration) return 0;
    if (elapsed<=duration-fade) return 255;
    return (u32)((duration-elapsed)*255/fade);
}

const char *portStateMessage(void) { return stateMessage; }

static int stateLayoutRead(void)
{
    char exe[2048], footer[8], magic[8]; u32 bytes, i, total = 0; FILE *f;
    if (stateLayout) return stateLayout > 0;
    stateLayout = -1;
#ifdef _WIN32
    { DWORD n=GetModuleFileNameA(NULL,exe,sizeof(exe)); if (!n || n>=sizeof(exe)) return 0; }
#else
    { int n = readlink("/proc/self/exe", exe, sizeof(exe)-1); if (n < 0) return 0; exe[n] = 0; }
#endif
    f = fopen(exe, "rb"); if (!f) return 0;
    if (fseek(f, -12, SEEK_END) || fread(footer, 1, 8, f) != 8 || memcmp(footer, "MIDASEND", 8)
        || fread(&bytes, 4, 1, f) != 1 || bytes < 44 || bytes > 44+STATE_MAX_REGIONS*8
        || fseek(f, -(long)(bytes+12), SEEK_END) || fread(magic, 1, 8, f) != 8
        || memcmp(magic, "MIDASLAY", 8) || fread(stateBuild, 1, 32, f) != 32
        || fread(&stateCount, 4, 1, f) != 1 || !stateCount || stateCount > STATE_MAX_REGIONS-3
        || bytes != 44+stateCount*8 || fread(stateRegions, 8, stateCount, f) != stateCount) {
        fclose(f); return 0;
    }
    fclose(f);
    for (i = 0; i < stateCount; i++) {
        if (!stateRegions[i].size || stateRegions[i].size > STATE_MAX_BYTES-total
            || stateRegions[i].addr+stateRegions[i].size < stateRegions[i].addr
            || (i && stateRegions[i].addr < stateRegions[i-1].addr+stateRegions[i-1].size)) return 0;
        total += stateRegions[i].size;
    }
    stateLayout = 1; return 1;
}

static void statePath(char *path, size_t cap, s32 folder, s32 slot, const char *suffix)
{
    char relative[128];
    snprintf(relative, sizeof(relative), "Data/SaveStates/profile-%d-slot-%d.mstate%s", folder, slot+1, suffix);
    snprintf(path, cap, "%s", portRootPath(relative));
}

static int stateHeader(StateHeader *h)
{
    u32 used, size, romSize;
    void *arena = portGetMempArena(&size), *high = portStateHighArena(&used);
    const u8 *rom = romdataRom(&romSize);
    if (!stateLayoutRead() || !arena || !high || !rom) return 0;
    memset(h, 0, sizeof(*h)); memcpy(h->magic, "MIDASSV1", 8);
    h->version = STATE_VERSION; h->headerSize = sizeof(*h);
    h->folder = selected_folder_num; h->stage = bossGetStageNum(); h->difficulty = lvlGetSelectedDifficulty();
    h->timestamp = (u32)time(NULL); h->seconds = g_CurrentPlayer ? getMissiontimer()/60 : 0;
    h->health = g_CurrentPlayer ? g_CurrentPlayer->bondhealth : 0;
    h->armour = g_CurrentPlayer ? g_CurrentPlayer->bondarmour : 0;
    h->count = stateCount+3; h->mainBase = (u32)arena; h->mainSize = size;
    h->highBase = (u32)high; h->highSize = used;
    h->romBase = (u32)rom; h->romSize = romSize; h->romCRC = romdataFingerprint();
    memcpy(h->build, stateBuild, 32);
    h->checksum = stateCRC(h, sizeof(*h)-sizeof(h->checksum)); return 1;
}

static int stateHeaderMatches(const StateHeader *h, const StateHeader *current)
{
    return !memcmp(h->magic, "MIDASSV1", 8)
        && h->version == STATE_VERSION && h->headerSize == sizeof(*h)
        && h->checksum == stateCRC(h, sizeof(*h)-sizeof(h->checksum))
        && strcmp(stateStageName(h->stage), "Mission") && h->difficulty <= 3
        && h->count == current->count && h->folder == current->folder
        && h->mainBase == current->mainBase && h->mainSize == current->mainSize
        && h->highBase == current->highBase && h->highSize <= 64u*1024u*1024u
        && h->romBase == current->romBase && h->romSize == current->romSize && h->romCRC == current->romCRC
        && !memcmp(h->build, current->build, 32);
}

static int stateCompatible(const StateHeader *h)
{
    StateHeader current;
    return stateHeader(&current) && stateHeaderMatches(h,&current);
}

s32 portStateInfo(s32 slot, PortStateInfo *info)
{
    char path[2048]; StateHeader h; FILE *f;
    memset(info, 0, sizeof(*info));
    if (slot < 0 || slot >= PORT_STATE_SLOTS) return 0;
    statePath(path, sizeof(path), selected_folder_num, slot, "");
    f = fopen(path, "rb"); if (!f) return 0;
    info->present = 1;
    if (fread(&h, sizeof(h), 1, f) == 1 && !memcmp(h.magic, "MIDASSV1", 8)) {
        info->present = 1; info->compatible = stateCompatible(&h); info->stage = h.stage;
        info->difficulty = h.difficulty; info->timestamp = h.timestamp; info->seconds = h.seconds;
        info->health = h.health; info->armour = h.armour;
    }
    fclose(f); return info->present;
}

/* Zero runs/literals, bounded to 32767 bytes. The file never supplies an
 * address: region descriptors are compared with the executable's registry. */
static int stateWriteRegion(FILE *f, const void *data, u32 size)
{
    const u8 *p = data; u32 at = 0, crc = stateCRC(data, size);
    if (fwrite(&crc, 4, 1, f) != 1) return 0;
    while (at < size) {
        u32 end = at; u16 code;
        if (p[at] == 0) {
            while (end < size && end-at < 32767 && p[end] == 0) end++;
            code = (u16)(end-at)|0x8000;
            if (fwrite(&code, 2, 1, f) != 1) return 0;
        } else {
            while (end < size && end-at < 32767 && (p[end] != 0 || (end+3 < size && (p[end+1] || p[end+2] || p[end+3])))) end++;
            code = (u16)(end-at);
            if (!code || fwrite(&code, 2, 1, f) != 1 || fwrite(p+at, 1, code, f) != code) return 0;
        }
        at = end;
    }
    return 1;
}

static int stateReadRegion(FILE *f, u8 *out, u32 size)
{
    u32 at = 0, crc;
    if (fread(&crc, 4, 1, f) != 1) return 0;
    while (at < size) {
        u16 code; u32 count;
        if (fread(&code, 2, 1, f) != 1 || !(count = code&32767) || count > size-at) return 0;
        if (code&32768) memset(out+at, 0, count);
        else if (fread(out+at, 1, count, f) != count) return 0;
        at += count;
    }
    return stateCRC(out, size) == crc;
}

static StateRegion stateRegion(const StateHeader *h, u32 i)
{
    StateRegion r;
    if (i < stateCount) return stateRegions[i];
    r.addr = i == stateCount ? h->mainBase : i == stateCount+1 ? h->highBase : h->romBase;
    r.size = i == stateCount ? h->mainSize : i == stateCount+1 ? h->highSize : h->romSize;
    return r;
}

#ifdef GE_VIDEO
static void stateAuditProbe(s32 slot, s32 verify)
{
    extern u64 g_randomSeed, g_tlbRandomSeed, g_chrObjRandomSeed;
    struct { u32 player, characters, timer, weapon; u64 rng[3]; } probe, expected;
    char path[128]; FILE *f;
    if (!getenv("GE_STATE_AUDIT")) return;
    memset(&probe,0,sizeof(probe));
    probe.player=stateCRC(g_CurrentPlayer,sizeof(*g_CurrentPlayer));
    probe.characters=stateCRC(g_ChrSlots,g_NumChrSlots*sizeof(*g_ChrSlots));
    probe.timer=getMissiontimer();probe.weapon=getCurrentPlayerWeaponId(GUNRIGHT);
    probe.rng[0]=g_randomSeed;probe.rng[1]=g_tlbRandomSeed;probe.rng[2]=g_chrObjRandomSeed;
    snprintf(path,sizeof(path),"state-probe-%d.bin",slot);
    f=fopen(portRootPath(path),verify ? "rb" : "wb");
    if (!f) exit(2);
    if (verify) {
        if (fread(&expected,sizeof(expected),1,f)!=1 || memcmp(&probe,&expected,sizeof(probe))) {
            fprintf(stderr,"state-audit: player/AI/timer/weapon/RNG probe mismatch (slot %d)\n",slot); exit(2);
        }
    } else if (fwrite(&probe,sizeof(probe),1,f)!=1) exit(2);
    fclose(f);
}
#endif

static int stateWrite(s32 slot)
{
    StateHeader h; char path[2048], temp[2048], backup[2048]; FILE *f; u32 i; int ok = 1;
    if (!stateHeader(&h)) { stateSay("Suspend points are unavailable in this build."); return 0; }
#ifdef GE_VIDEO
    { extern void portRenderStateBarrier(void); portRenderStateBarrier(); }
#endif
#ifdef _WIN32
    _mkdir(portRootPath("Data/SaveStates"));
#else
    mkdir(portRootPath("Data/SaveStates"), 0755);
#endif
    statePath(path, sizeof(path), h.folder, slot, "");
    statePath(temp, sizeof(temp), h.folder, slot, ".tmp");
    statePath(backup, sizeof(backup), h.folder, slot, ".bak");
    f = fopen(temp, "wb"); if (!f) { stateSay("Could not create suspend point."); return 0; }
    ok = fwrite(&h, sizeof(h), 1, f) == 1;
    for (i = 0; ok && i < h.count; i++) {
        StateRegion r = stateRegion(&h, i);
        ok = fwrite(&r, sizeof(r), 1, f) == 1 && stateWriteRegion(f, (const void *)r.addr, r.size);
    }
    if (fflush(f)) ok = 0;
#ifdef _WIN32
    if (ok && _commit(_fileno(f))) ok=0;
#else
    if (ok && fsync(fileno(f))) ok=0;
#endif
    if (fclose(f)) ok = 0;
    if (!ok) { remove(temp); stateSay("Suspend point write failed; previous save kept."); return 0; }
#ifdef _WIN32
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES && !CopyFileA(path, backup, FALSE)) {
        remove(temp); stateSay("Could not back up previous save."); return 0;
    }
    if (!MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        stateSay("Could not replace suspend point; previous save kept."); return 0;
    }
#else
    remove(backup);
    link(path, backup);
    if (rename(temp, path)) { stateSay("Could not replace suspend point; previous save kept."); return 0; }
#endif
#ifdef GE_VIDEO
    stateAuditProbe(slot,0);
#endif
    /* Capturing the automatic safety image must not announce a manual save. */
    if (slot!=PORT_STATE_UNDO) stateNotify(slot,0,h.stage);
    return 1;
}

static void stateFree(StateImage *image)
{
    u32 i; if (image->data) { for (i = 0; i < image->h.count; i++) free(image->data[i]); free(image->data); }
    memset(image, 0, sizeof(*image));
}

static int stateRead(s32 slot, StateImage *image)
{
    char path[2048]; FILE *f; u32 i, total = 0; int ok;
    statePath(path, sizeof(path), selected_folder_num, slot, "");
    f = fopen(path, "rb"); if (!f) { stateSay("That suspend-point slot is empty."); return 0; }
    ok = fread(&image->h, sizeof(image->h), 1, f) == 1 && stateCompatible(&image->h);
    if (!ok) {
        if (getenv("GE_STATE_AUDIT")) {
            StateHeader current; stateHeader(&current);
            fprintf(stderr,"state-audit: compatibility folder %u/%u main %x/%x high %x/%x ROM %x/%x count %u/%u checksum %x/%x\n",image->h.folder,current.folder,image->h.mainBase,current.mainBase,image->h.highBase,current.highBase,image->h.romBase,current.romBase,image->h.count,current.count,image->h.checksum,stateCRC(&image->h,sizeof(StateHeader)-4));
        }
        fclose(f); stateSay("Suspend point is incompatible with this build, ROM, or memory layout."); return 0;
    }
    image->data = calloc(image->h.count, sizeof(*image->data));
    if (!image->data) ok = 0;
    for (i = 0; ok && i < image->h.count; i++) {
        StateRegion actual, expected = stateRegion(&image->h, i);
        ok = fread(&actual, sizeof(actual), 1, f) == 1 && actual.addr == expected.addr && actual.size == expected.size
            && actual.size <= STATE_MAX_BYTES-total;
        if (!ok) break;
        total += actual.size; image->data[i] = malloc(actual.size ? actual.size : 1);
        ok = image->data[i] && stateReadRegion(f, image->data[i], actual.size);
    }
    if (ok && fgetc(f) != EOF) ok = 0;
    fclose(f);
    if (!ok) { stateFree(image); stateSay("Suspend point is damaged; current mission kept."); }
    return ok;
}

static void stateRestoreComplete(u32 stage)
{
    stateOpen=stateResumePending=statePause=0;
    if (g_CurrentPlayer && g_CurrentPlayer->watch_animation_state==WATCH_ANIMATION_0x5)
        set_open_close_solo_watch_menu_to1();
    stateRestoreSerial++;
    stateNotify(stateSourceSlot,1,stage);
}

static void stateApply(StateImage *image)
{
    u32 i;
#ifdef GE_VIDEO
    { extern void portRenderStateBarrier(void); portRenderStateBarrier(); }
#endif
    for (i = 0; i < image->h.count; i++) {
        StateRegion r = stateRegion(&image->h, i); memcpy((void *)r.addr, image->data[i], r.size);
    }
    portStateHighWater(image->h.highSize);
#ifdef GE_VIDEO
    stateAuditProbe(stateSourceSlot,1);
#endif
    portStatePresentationReset();
    stateRestoreComplete(image->h.stage);
    stateFree(image);
}

void portStateRequest(s32 slot, s32 load)
{
    if (slot >= 0 && slot < PORT_STATE_SLOTS && !stateWaiting) { stateCommand = slot; stateLoad = load; }
}

void portStateStageReady(void)
{
    if (stateWaiting && (u32)bossGetStageNum() == statePending.h.stage) {
        stateWaiting = 0; stateApply(&statePending);
    } else {
        stateOpen=statePause=stateResumePending=0; stateCommand=-1;
        if (stateWaiting) {
            stateWaiting=0;stateFree(&statePending);
            stateSay("Mission load failed. The previous mission is in Undo Load.");
        }
    }
}

#ifdef GE_VIDEO
/* Isolated end-to-end audit; never runs without an explicit environment knob.
 * The launcher uses its own Data folder, including a second fresh process. */
static void stateAudit(void)
{
    static int phase, ticks, frames, savedTimer, manual;
    static u32 restored;
    static coord3d savedPos;
    const char *mode = getenv("GE_STATE_AUDIT");
    extern int ge_shot_force;
    FILE *log;
    int i;
    if (!mode) return;
    if (++frames > 4500) { fprintf(stderr,"state-audit: timeout phase %d\n",phase); exit(2); }
    if (stateWaiting) return;
    if (phase == 0 && (portBondControlsLive() || (!strcmp(mode,"read") && g_CurrentPlayer && bossGetStageNum()!=LEVELID_TITLE)
        || (!strcmp(mode,"title") && bossGetStageNum()==LEVELID_TITLE && frames>120)) && ++ticks > 120) {
        if (!strcmp(mode,"reject")) {
            u32 i, before=0, after=0; StateHeader h; StateImage invalid={0};
            stateHeader(&h);
            for (i=0;i<h.count;i++) { StateRegion r=stateRegion(&h,i); before^=stateCRC((void *)r.addr,r.size); }
            if (stateRead(PORT_STATE_QUICK,&invalid)) { fprintf(stderr,"state-audit: damaged image accepted\n");exit(2); }
            for (i=0;i<h.count;i++) { StateRegion r=stateRegion(&h,i); after^=stateCRC((void *)r.addr,r.size); }
            if (before!=after) { fprintf(stderr,"state-audit: rejected load changed world\n");exit(2); }
            phase=7;ticks=0;return;
        }
        if (!strcmp(mode,"read") || !strcmp(mode,"title")) {
            portStateRequest(PORT_STATE_QUICK,1); phase=3; ticks=0; return;
        }
        g_CurrentPlayer->bondhealth=0.625f; g_CurrentPlayer->bondarmour=0.5f;
        for (i=0;i<4;i++) if (!stateWrite(i)) exit(2);
        if (!strcmp(mode,"write")) { ge_shot_force=1;phase=16;ticks=0;return; }
        if (!stateWrite(PORT_STATE_QUICK)) exit(2);
        phase=1; ticks=0;
    } else if (phase == 1 && ++ticks > 90) {
        if (!strcmp(mode,"watch")) { trigger_solo_watch_menu(0); phase=11; return; }
        g_CurrentPlayer->bondhealth=0.25f; g_CurrentPlayer->bondarmour=0.125f;
        portStateRequest(PORT_STATE_QUICK,1); phase=3; ticks=0;
    } else if (phase == 3 && stateRestoreSerial!=restored) {
        restored=stateRestoreSerial;
        if (portStatePaused()) { fprintf(stderr,"state-audit: restore paused gameplay\n");exit(2); }
        if (g_CurrentPlayer->bondhealth != 0.625f || g_CurrentPlayer->bondarmour != 0.5f) {
            fprintf(stderr,"state-audit: quickload mismatch %f %f\n",g_CurrentPlayer->bondhealth,g_CurrentPlayer->bondarmour); exit(2);
        }
        savedTimer=getMissiontimer();savedPos=g_CurrentPlayer->prop->pos;
        ge_shot_force=1;phase=4; ticks=0;
    } else if (phase == 3 && ++ticks > 10) {
        fprintf(stderr,"state-audit: load rejected: %s\n",stateMessage); exit(2);
    } else if (phase == 4 && ++ticks > 120) {
        if (!portBondControlsLive() || getMissiontimer()<=savedTimer
            || (g_CurrentPlayer->prop->pos.x==savedPos.x && g_CurrentPlayer->prop->pos.z==savedPos.z)) {
            fprintf(stderr,"state-audit: mission did not resume/move\n");exit(2);
        }
        if (!strcmp(mode,"read") || !strcmp(mode,"title") || !strcmp(mode,"watch")) { phase=6; ticks=0; ge_shot_force=1; }
        else { portStateRequest(PORT_STATE_UNDO,1); phase=5; }
    } else if (phase == 5 && stateRestoreSerial!=restored) {
        restored=stateRestoreSerial;
        if (g_CurrentPlayer->bondhealth != 0.25f || g_CurrentPlayer->bondarmour != 0.125f) {
            fprintf(stderr,"state-audit: undo mismatch\n"); exit(2);
        }
        ge_shot_force=1;phase=6; ticks=0;
    } else if (phase == 6 && ++ticks > 120) {
        if (!strcmp(mode,"roundtrip")) { portStateRequest(0,1); phase=8;ticks=0;return; }
        if (!strcmp(mode,"read")) {
            /* Revisiting an earlier stage must convert its restored raw ROM. */
            set_solo_and_ptr_briefing(LEVELID_FACILITY);
            bossSetLoadedStage(LEVELID_FACILITY);phase=13;ticks=0;return;
        }
        ge_shot_force=1; phase=7; ticks=0;
    } else if (phase == 8 && stateRestoreSerial!=restored) {
        restored=stateRestoreSerial;ge_shot_force=1;
        if (g_CurrentPlayer->bondhealth!=0.625f) exit(2);
        if (++manual < 4) portStateRequest(manual,1);
        else {
            /* Recovery is available even after death, without saving over the
             * live checkpoint. Undo may deliberately return to that death. */
            g_CurrentPlayer->bonddead=1;
            portStateRequest(PORT_STATE_QUICK,1);phase=9;
        }
    } else if (phase == 9 && stateRestoreSerial!=restored) {
        restored=stateRestoreSerial;
        if (g_CurrentPlayer->bonddead) exit(2);
        phase=10;ticks=0;
    } else if (phase == 10 && ++ticks > 120) {
        ge_shot_force=1;phase=7;ticks=0;
    } else if (phase == 11 && g_CurrentPlayer->watch_animation_state==WATCH_ANIMATION_0x5) {
        portStateMenuOpen();portStateRequest(PORT_STATE_QUICK,0);phase=12;
    } else if (phase == 12) {
        portStateRequest(PORT_STATE_QUICK,1);phase=3;ticks=0;
    } else if (phase == 13 && bossGetStageNum()==LEVELID_FACILITY && ++ticks > 90) {
        if (!g_CurrentPlayer || g_NumChrSlots<=0) exit(2);
        portStateRequest(PORT_STATE_QUICK,1);phase=14;
    } else if (phase == 14 && stateRestoreSerial!=restored) {
        restored=stateRestoreSerial;phase=10;ticks=0;
    } else if (phase == 16 && ++ticks>5) {
        if (!stateWrite(PORT_STATE_QUICK)) exit(2);
        ge_shot_force=1;phase=17;ticks=0;
    } else if (phase == 17 && ++ticks>5) {
        portStateMenuOpen();ge_shot_force=1;phase=7;ticks=0;
    } else if (phase == 7 && ++ticks > 5) {
        log=fopen(portRootPath("state-audit-ok.txt"),"w");
        if (log) { fprintf(log,"PASS %s: stage %d, health %f, armour %f, mission %d\n",mode,bossGetStageNum(),g_CurrentPlayer->bondhealth,g_CurrentPlayer->bondarmour,getMissiontimer()); fclose(log); }
        exit(0);
    }
}
#endif

void portStateFrame(void)
{
    s32 slot;
    extern void portStateInput(void);
    /* Resume only after a fresh input sample. The confirming controller
     * button belongs to the menu, even when held into the next game tick. */
    if (stateResumePending) {
        stateResumePending=statePause=0;
        if (g_CurrentPlayer && g_CurrentPlayer->watch_animation_state==WATCH_ANIMATION_0x5)
            set_open_close_solo_watch_menu_to1();
    }
    portStateInput();
#ifdef GE_VIDEO
    { extern void portStateEffectsAudit(u32); portStateEffectsAudit(stateRestoreSerial); }
    stateAudit();
#endif
    slot = stateCommand;
    if (slot < 0 || stateWaiting) return;
    stateCommand = -1;
    if (bossGetStageNum()!=LEVELID_TITLE && getPlayerCount() > 1) {
        stateSay("Suspend points are available during solo missions."); return;
    }
    { extern s32 get_is_ramrom_flag(void);
      if (bossGetStageNum()!=LEVELID_TITLE && get_is_ramrom_flag()) { stateSay("Suspend points are unavailable during demo playback."); return; } }
    if (!stateLoad) {
        if (bossGetStageNum() == LEVELID_TITLE || !g_CurrentPlayer || g_CurrentPlayer->bonddead
            || g_CurrentPlayer->cameramode
            || (lvlGetControlsLockedFlag() && g_CurrentPlayer->watch_animation_state!=WATCH_ANIMATION_0x5)
            || slot == PORT_STATE_UNDO) {
            stateSay("Save while Bond is alive and in control."); return;
        }
        stateWrite(slot); return;
    }
    if (!stateRead(slot, &statePending)) return;
    stateSourceSlot=slot;
    if (slot != PORT_STATE_UNDO && bossGetStageNum() != LEVELID_TITLE && !stateWrite(PORT_STATE_UNDO)) { stateFree(&statePending); return; }
    if ((u32)bossGetStageNum() != statePending.h.stage) {
        stateWaiting = 1; stateOpen = statePause = 0;
        lvlSetSelectedDifficulty(statePending.h.difficulty);
        set_selected_difficulty(statePending.h.difficulty);
        set_solo_and_ptr_briefing(statePending.h.stage);
        bossSetLoadedStage(statePending.h.stage);
    } else stateApply(&statePending);
}

void portStatePresentationReset(void)
{
    extern void portStateResetInput(void);
    portStateResetInput();
    portBindCooldownStart();
    portHudReset(0);
    portMissionHintsDismiss();
    if (g_CurrentPlayer) {
        g_CurrentPlayer->insightaimmode = 0;
    }
#ifdef GE_VIDEO
    {
        extern void gfx_texture_cache_clear_c(void);
        extern void portAudioStateReset(void);
        gfx_texture_cache_clear_c(); portAudioStateReset();
    }
#endif
}

static const char *stateStageName(u32 stage)
{
    static const struct { s32 stage; const char *name; } names[] = {
        {LEVELID_DAM,"Dam"}, {LEVELID_FACILITY,"Facility"}, {LEVELID_RUNWAY,"Runway"},
        {LEVELID_SURFACE,"Surface"}, {LEVELID_BUNKER1,"Bunker"}, {LEVELID_SILO,"Silo"},
        {LEVELID_FRIGATE,"Frigate"}, {LEVELID_STATUE,"Statue"}, {LEVELID_ARCHIVES,"Archives"},
        {LEVELID_STREETS,"Streets"}, {LEVELID_DEPOT,"Depot"}, {LEVELID_TRAIN,"Train"},
        {LEVELID_JUNGLE,"Jungle"}, {LEVELID_CONTROL,"Control"}, {LEVELID_CAVERNS,"Caverns"},
        {LEVELID_CRADLE,"Cradle"}, {LEVELID_AZTEC,"Aztec"}, {LEVELID_EGYPT,"Egypt"},
        {LEVELID_SURFACE2,"Surface 2"}, {LEVELID_BUNKER2,"Bunker 2"}
    };
    s32 i; for (i=0; i<sizeof(names)/sizeof(names[0]); i++) if (stage==names[i].stage) return names[i].name;
    return "Mission";
}

static Gfx *stateDrawToast(Gfx *g)
{
    const u32 alpha=stateToastAlpha(osGetTime());
    const s32 available=viGetViewWidth()-24, width=available<238 ? available : 238;
    const s32 x=viGetViewLeft()+(viGetViewWidth()-width)/2, y=viGetViewTop()+3;
    if (!alpha || width<=16) return g;
    g=portHudPanel(g,x,y,width,17,0x10171a00|alpha*230/255);
    g=portHudPanel(g,x,y,2,17,0xb8de8600|alpha);
    return portHudLabel(g,stateToast,x+8,y+4,0.55f,0xf2ffdc00|alpha,width-16);
}

Gfx *portStateRender(Gfx *g)
{
    static PortStateInfo info[PORT_STATE_SLOTS];
    static u32 refresh;
    u32 now = (u32)time(NULL);
    s32 x = viGetViewLeft()+14, y = viGetViewTop()+22, width = viGetViewWidth()-28, i;
    char line[160];
    if (!stateOpen) { refresh = 0; return stateDrawToast(g); }
    g = portHudPanel(g, x, y, width, 196, 0x10171af5);
    g = portHudLabel(g, "SUSPEND POINTS", x+10, y+8, 0.8f, 0xd7f3a9ff, width-20);
    if (refresh != now) {
        for (i=0; i<PORT_STATE_SLOTS; i++) portStateInfo(i, &info[i]);
        refresh = now;
    }
    g = portHudLabel(g, stateMessage, x+10, y+22, 0.45f, 0xc3ccd4ff, width-20);
    for (i=0; i<PORT_STATE_SLOTS; i++) {
        const s32 top = y+36+i*23, active = i==stateRow;
        const char *slot = i<4 ? "Slot" : i==4 ? "Quicksave" : "Undo load";
        if (active) g = portHudPanel(g, x+8, top, width-16, 22, 0x354734ee);
        if (i<4) snprintf(line,sizeof(line),"%s %d: %s",slot,i+1,info[i].present ? stateStageName(info[i].stage) : "Empty");
        else snprintf(line,sizeof(line),"%s: %s",slot,info[i].present ? stateStageName(info[i].stage) : "Empty");
        g = portHudLabel(g,line,x+12,top+2,0.58f,0xf2ffdcff,width-106);
        if (info[i].present) {
            time_t stamp=info[i].timestamp; struct tm *tm=localtime(&stamp); char date[40];
            if (tm) strftime(date,sizeof(date),"%m/%d %H:%M",tm); else date[0]=0;
            const char *difficulty=info[i].difficulty==0 ? "Agent" : info[i].difficulty==1 ? "Secret" : info[i].difficulty==2 ? "00" : "007";
            snprintf(line,sizeof(line),"%s  %s %u:%02u%s",date,difficulty,info[i].seconds/60,info[i].seconds%60,info[i].compatible ? "" : "  Incompatible");
            g=portHudLabel(g,line,x+12,top+12,0.4f,0xc3ccd4ff,width-106);
        }
        if (i!=PORT_STATE_UNDO) g=portHudLabel(g,"Save",x+width-80,top+7,0.55f,active && !stateChoice ? 0xd7f3a9ff : 0x889096ff,34);
        g=portHudLabel(g,"Load",x+width-43,top+7,0.55f,active && (stateChoice || i==PORT_STATE_UNDO) ? 0xd7f3a9ff : 0x889096ff,34);
    }
    g=portHudLabel(g,"Arrows: choose   Enter: accept   Esc: resume",x+10,y+180,0.43f,0xc3ccd4ff,width-20);
    return stateDrawToast(g);
}

void portStateMenuOpen(void)
{
    if (bossGetStageNum()!=LEVELID_TITLE && getPlayerCount()>1) { stateSay("Suspend points are available during solo missions."); return; }
    stateOpen = !stateOpen; stateConfirm = 0;
    portBindCooldownStart();
    if (!stateOpen) { statePause=1;stateResumePending=1; }
}
s32 portStatePaused(void) { return stateOpen || statePause; }

/* Logical menu keys: up/down/left/right/accept/back. SDL stays in input_pc.c. */
s32 portStateMenuInput(s32 key)
{
    if (!portStatePaused()) return 0;
    if (key>=5) portBindCooldownStart();
    if (!stateOpen) {
        stateResumePending=1;
        return 1;
    }
    if (key == 1) stateRow = (stateRow+PORT_STATE_SLOTS-1)%PORT_STATE_SLOTS;
    if (key == 2) stateRow = (stateRow+1)%PORT_STATE_SLOTS;
    if (key == 3 || key == 4) stateChoice ^= 1;
    if (key == 6) { stateOpen=stateConfirm=0;statePause=stateResumePending=1;return 1; }
    if (key != 5) { stateConfirm = 0; return 1; }
    if (stateRow == PORT_STATE_UNDO) stateChoice = 1;
    if (!stateChoice && !stateConfirm) {
        PortStateInfo info;
        if (portStateInfo(stateRow, &info)) { stateConfirm = 1; stateSay("Press Accept again to overwrite this slot."); return 1; }
    }
    stateConfirm = 0; portStateRequest(stateRow, stateChoice); return 1;
}
