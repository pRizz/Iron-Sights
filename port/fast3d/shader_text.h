/* Keep the real destination capacity through all shader construction helpers. */
#ifndef MIDAS_SHADER_TEXT_H
#define MIDAS_SHADER_TEXT_H
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "system.h"

template<size_t N>
static int shader_format(char (&buffer)[N], size_t used, const char *format, va_list args)
{
    if (used >= N) sysFatalError("Shader text offset exceeds its buffer");
    const int count = std::vsnprintf(buffer + used, N - used, format, args);
    if (count < 0 || size_t(count) >= N - used) sysFatalError("Shader text buffer exhausted");
    return count;
}

template<size_t N>
static int shader_appendf(char (&buffer)[N], size_t used, const char *format, ...)
{
    va_list args; va_start(args, format);
    const int count = shader_format(buffer, used, format, args);
    va_end(args);
    return count;
}

template<size_t N>
static int f3d_sprintf(char (&buffer)[N], const char *format, ...)
{
    va_list args; va_start(args, format);
    const int count = shader_format(buffer, 0, format, args);
    va_end(args);
    return count;
}

template<size_t N>
static void append_str(char (&buffer)[N], size_t *used, const char *text)
{
    const size_t count = std::strlen(text);
    if (*used >= N || count >= N - *used) sysFatalError("Shader text buffer exhausted");
    std::memcpy(buffer + *used, text, count + 1);
    *used += count;
}

template<size_t N>
static void append_line(char (&buffer)[N], size_t *used, const char *text)
{
    append_str(buffer, used, text);
    append_str(buffer, used, "\n");
}
#endif
