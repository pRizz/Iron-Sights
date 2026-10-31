#ifndef PORT_MEMP_H
#define PORT_MEMP_H
#include <memp.h>
MemoryPool *portMempPool(u8 bank);
void *portMempAllocate(u32 bytes, u8 bank);
MEMP_ADD_ENTRY_RESULT portMempResize(void *allocation, s32 size, u8 bank);
#endif
