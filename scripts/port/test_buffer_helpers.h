#ifndef TEST_BUFFER_HELPERS_H
#define TEST_BUFFER_HELPERS_H
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
void osSyncPrintf(const char *format, ...)
{
    va_list args; va_start(args, format); vfprintf(stderr, format, args); va_end(args); abort();
}
void portConLogf(const char *format, ...) { (void)format; }
#endif
