/* The option table's text accessors (port_options.c) that game code calls:
 * front.c's multiplayer settings sheet reads and sets rows by index. */
#ifndef PORT_OPTIONS_H
#define PORT_OPTIONS_H

#include <ultra64.h>

/* Row i's current value as console text into buf (n bytes); 0 when it can't be read. */
s32 portOptValueIni(s32 i, char *buf, s32 n);
/* Sets row i from text, applies it live and saves; 0 if the text didn't parse. */
s32 portOptSetIni(s32 i, const char *text);

#endif
