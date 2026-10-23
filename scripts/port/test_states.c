/* ROM-free codec and compatibility checks. The runtime audit covers worlds. */
#include <assert.h>
#include <stdarg.h>
#include "port/src/port_state.c"

void osSyncPrintf(const char *fmt, ...)
{
    va_list args; va_start(args,fmt);vfprintf(stderr,fmt,args);va_end(args);abort();
}

static void checksum(StateHeader *h) { h->checksum=stateCRC(h,sizeof(*h)-4); }

/* Menu tests use an isolated metadata file, never a real campaign. */
static char menuPath[128];
static int cooldowns;
static int popup;
static OSTime testNow=1;
s32 selected_folder_num;
struct player *g_CurrentPlayer;
const char *portRootPath(const char *name) { (void)name;return menuPath; }
void portConPrintf(s32 show,const char *fmt,...) { popup=show;(void)fmt; }
OSTime osGetTime(void) { return testNow; }
void set_open_close_solo_watch_menu_to1(void) { g_CurrentPlayer->open_close_solo_watch_menu=1; }
void portBindCooldownStart(void) { cooldowns++; }
void *portGetMempArena(u32 *size) { *size=0;return NULL; }
void *portStateHighArena(u32 *size) { *size=0;return NULL; }
const u8 *romdataRom(u32 *size) { *size=0;return NULL; }
u32 romdataFingerprint(void) { return 0; }
LEVELID bossGetStageNum(void) { return LEVELID_DAM; }
DIFFICULTY lvlGetSelectedDifficulty(void) { return DIFFICULTY_AGENT; }
s32 getMissiontimer(void) { return 0; }

int main(void)
{
    u8 *data=malloc(200000), *out=malloc(200000);
    unsigned seed=1337, i, kind;
    FILE *f;
    StateHeader h={0}, other;
    assert(data && out);
    assert(stateCRC("123456789",9)==0xcbf43926);
    for (kind=0;kind<5;kind++) {
        for (i=0;i<200000;i++) {
            seed=seed*1664525u+1013904223u;
            data[i]=kind==0 ? 0 : kind==1 ? 255 : kind==2 ? seed>>24 : kind==3 ? (i%40000 < 34000 ? 0 : seed>>24) : i%7;
        }
        f=tmpfile(); assert(f && stateWriteRegion(f,data,200000));
        rewind(f); assert(stateReadRegion(f,out,200000)); assert(!memcmp(data,out,200000));
        /* A truncated stream cannot complete, even for the all-zero case. */
        rewind(f); { FILE *cut=tmpfile(); int c; long end; fseek(f,0,SEEK_END); end=ftell(f); rewind(f);
            for(i=0;i<(u32)end-1;i++) {c=fgetc(f); fputc(c,cut);} rewind(cut);
            assert(!stateReadRegion(cut,out,200000)); fclose(cut); }
        /* CRC protects both run and literal payloads. */
        rewind(f); fputc(0,f); fputc(0,f); fputc(0,f); fputc(0,f); rewind(f);
        assert(!stateReadRegion(f,out,200000)); fclose(f);
    }
    f=tmpfile(); {u32 crc=0;u16 code=0xffff; fwrite(&crc,4,1,f);fwrite(&code,2,1,f);} rewind(f);
    assert(!stateReadRegion(f,out,100)); fclose(f);
    f=tmpfile(); {u32 crc=0;u16 code=0; fwrite(&crc,4,1,f);fwrite(&code,2,1,f);} rewind(f);
    assert(!stateReadRegion(f,out,100)); fclose(f);
    memcpy(h.magic,"MIDASSV1",8); h.version=STATE_VERSION;h.headerSize=sizeof(h);
    h.stage=LEVELID_DAM;h.count=85;h.folder=0;h.romCRC=0xb6330846;h.highSize=4000000; checksum(&h);
    assert(stateHeaderMatches(&h,&h));
    other=h;other.build[0]^=1;checksum(&other);assert(!stateHeaderMatches(&other,&h));
    other=h;other.romCRC^=1;checksum(&other);assert(!stateHeaderMatches(&other,&h));
    other=h;other.folder=1;checksum(&other);assert(!stateHeaderMatches(&other,&h));
    other=h;other.highSize=65u*1024u*1024u;checksum(&other);assert(!stateHeaderMatches(&other,&h));
    other=h;other.stage=0xffffffff;checksum(&other);assert(!stateHeaderMatches(&other,&h));
    other=h;other.difficulty=4;checksum(&other);assert(!stateHeaderMatches(&other,&h));
    other=h;other.mainBase++;checksum(&other);assert(!stateHeaderMatches(&other,&h));
    other=h;other.seconds++;assert(!stateHeaderMatches(&other,&h));
    snprintf(menuPath,sizeof(menuPath),"/tmp/midas-state-menu-%ld",(long)getpid());
    f=fopen(menuPath,"wb");assert(f);assert(fwrite(&h,sizeof(h),1,f)==1);fclose(f);
    stateOpen=1;statePause=0;stateRow=stateChoice=stateConfirm=0;stateCommand=-1;
    portStateMenuInput(5);assert(stateConfirm && stateCommand==-1 && stateOpen);
    portStateMenuInput(4);assert(!stateConfirm && stateChoice); /* changing action disarms overwrite */
    portStateMenuInput(3);portStateMenuInput(5);assert(stateConfirm && stateCommand==-1);
    portStateMenuInput(5);assert(!stateConfirm && stateCommand==0 && !stateLoad && stateOpen);
    portStateMenuInput(6);assert(!stateOpen && statePause && stateResumePending && cooldowns>0);
    /* A resume press cannot run gameplay with that same input sample. */
    stateResumePending=0;statePause=1;portStateMenuInput(5);
    assert(statePause && stateResumePending);
    {
        struct player player={0};
        g_CurrentPlayer=&player;
        stateSourceSlot=1;stateOpen=statePause=stateResumePending=1;
        stateRestoreComplete(LEVELID_DAM);
        assert(!portStatePaused() && !stateResumePending && stateRestoreSerial==1);
        assert(!strcmp(stateToast,"Loaded slot 2 - Dam") && popup==0);
        assert(stateToastAlpha(testNow)==255);
        assert(stateToastAlpha(testNow+117187500ull)==255); /* 2.5 seconds */
        assert(stateToastAlpha(testNow+128906250ull)>=127 && stateToastAlpha(testNow+128906250ull)<=128);
        assert(stateToastAlpha(testNow+140625000ull)==0); /* Three seconds, no frame-rate dependency */
        assert(stateToastAlpha(testNow+200000000ull)==0);
        testNow+=150000000ull;
        stateNotify(PORT_STATE_QUICK,0,LEVELID_FACILITY);
        assert(!strcmp(stateToast,"Quicksaved - Facility") && stateToastAlpha(testNow)==255);
        stateNotify(3,0,LEVELID_DAM);assert(!strcmp(stateToast,"Saved slot 4 - Dam"));
        stateNotify(PORT_STATE_QUICK,1,LEVELID_DAM);assert(!strcmp(stateToast,"Loaded quicksave - Dam"));
        player.watch_animation_state=WATCH_ANIMATION_0x5;
        stateSourceSlot=PORT_STATE_UNDO;stateOpen=statePause=stateResumePending=1;
        stateRestoreComplete(LEVELID_DAM);
        assert(!portStatePaused() && player.open_close_solo_watch_menu);
        assert(!strcmp(stateToast,"Undo load restored - Dam"));
        g_CurrentPlayer=NULL;
    }
    assert(remove(menuPath)==0);
    free(data);free(out);puts("Suspend codec, compatibility, immediate restore and toast tests passed.");return 0;
}
