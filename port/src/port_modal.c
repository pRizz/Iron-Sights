#include "port_modal.h"
#ifdef _WIN32
#include <windows.h>
static volatile LONG modalDepth;
void portModalBegin(void) { InterlockedIncrement(&modalDepth); }
void portModalEnd(void) { InterlockedDecrement(&modalDepth); }
int portModalActive(void) { return InterlockedCompareExchange(&modalDepth,0,0)>0; }
#else
void portModalBegin(void) {}
void portModalEnd(void) {}
int portModalActive(void) { return 0; }
#endif
