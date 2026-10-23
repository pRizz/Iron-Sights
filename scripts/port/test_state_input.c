/* Menu routing, including pads which report D-pad as both stick and buttons. */
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include "game/gun.h"
/* The freestanding game headers omit these hosted prototypes. */
extern int strcmp(const char *,const char *);
extern int strncmp(const char *,const char *,size_t);
extern void *memset(void *,int,size_t);
extern s32 get_itemtype_in_hand(GUNHAND hand);
#include "port/src/joy_port.c"
static s32 paused, opens, keyCount, lastKey, requests, slot, load;
static s8 stickX,stickY;
static u16 buttons;
static OSTime now=1;
OSTime osGetTime(void) { return now; }
s8 joyGetStickX(s8 pad) { (void)pad;return stickX; }
s8 joyGetStickY(s8 pad) { (void)pad;return stickY; }
u16 joyGetButtonsPressedThisFrame(s8 pad,u16 mask) { (void)pad;return buttons&mask; }
void portStateMenuOpen(void) { paused=!paused;opens++; }
s32 portStatePaused(void) { return paused; }
s32 portStateMenuInput(s32 key) { keyCount++;lastKey=key;if(key==6)paused=0;return 1; }
void portStateRequest(s32 s,s32 l) { requests++;slot=s;load=l; }
void osSyncPrintf(const char *fmt,...) { va_list args;va_start(args,fmt);vfprintf(stderr,fmt,args);va_end(args);abort(); }
static void press(s32 action) { sLatchPoll[0][action]=1;sLatchN[0][action]=1;sLatchCount[0][action]=now; }
int main(void)
{
    press(PORTACT_STATE_MENU);portStateInput();assert(paused && opens==1);
    stickY=85;buttons=U_JPAD;portStateInput();assert(keyCount==1 && lastKey==1);
    buttons=0;portStateInput();assert(keyCount==1); /* held stick cannot repeat an accept/navigation edge */
    stickY=0;portStateInput();stickY=-85;portStateInput();assert(keyCount==2 && lastKey==2);
    stickY=0;portStateInput();
    press(PORTACT_MENU_ACCEPT);portStateInput();assert(keyCount==3 && lastKey==5);
    press(PORTACT_NEXT_INV);portStateInput();assert(keyCount==4 && lastKey==6 && !paused); /* default pad Back */
    press(PORTACT_QUICKSAVE);portStateInput();assert(requests==1 && slot==4 && !load);
    portStateInput();assert(requests==1);
    press(PORTACT_QUICKLOAD);portStateInput();assert(requests==2 && slot==4 && load);
    press(PORTACT_UNDO_LOAD);portStateInput();assert(requests==3 && slot==5 && load);
    press(PORTACT_QUICKLOAD);now+=251ull*46875ull;portStateInput();assert(requests==3);
    puts("Suspend input routing tests passed.");return 0;
}
