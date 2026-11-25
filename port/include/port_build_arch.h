#ifndef PORT_BUILD_ARCH_H
#define PORT_BUILD_ARCH_H

/* The architecture this build was compiled for, as the crash reports'
 * `architecture` tag, the 64-bit `ge: sys: build` log line and
 * build-info.json name it (LP64-007 Task 2): "x86_64", "i686", "aarch64",
 * "wasm32", else "unknown". It reads only the compiler's predefined macros,
 * so one source names each package. scripts/port/buildinfo.cmake gives
 * build-info.json the same names. */
static inline const char *portBuildArchitecture(void)
{
#if defined(__wasm32__) || defined(__EMSCRIPTEN__)
    return "wasm32";
#elif defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "i686";
#elif defined(__aarch64__)
    return "aarch64";
#else
    return "unknown";
#endif
}

#endif
