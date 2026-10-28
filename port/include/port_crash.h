#ifndef PORT_CRASH_H
#define PORT_CRASH_H

#ifdef _WIN32
#include <windows.h>
void portCrashInit(LPTOP_LEVEL_EXCEPTION_FILTER local_filter);
void portCrashReportStall(void);
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
void portCrashInit(void);
#endif
#if defined(_WIN32) || (defined(__linux__) && !defined(__EMSCRIPTEN__))
void portCrashShutdown(void);
int portCrashOptionGet(const char *key, int *out);
int portCrashOptionSet(const char *key, int value);
int portCrashTestMode(int argc, char **argv);
#endif

#endif
