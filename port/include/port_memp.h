#ifndef PORT_MEMP_H
#define PORT_MEMP_H
#include <memp.h>
#include "platform_info.h"
MemoryPool *portMempPool(u8 bank);
void *portMempAllocate(u32 bytes, u8 bank);
MEMP_ADD_ENTRY_RESULT portMempResize(void *allocation, s32 size, u8 bank);
#if IS_64_BIT
/* Stops the game, naming the bank, when START is not 8-aligned (64 bits only;
 * mempSetBankStarts calls it for each bank it sets). */
void portMempCheckBankStart(u8 bank, const void *start);
#endif
#endif
