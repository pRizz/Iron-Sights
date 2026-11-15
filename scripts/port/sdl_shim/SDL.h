/* The slice of SDL that port_baked_trace.c uses, for test_baked_trace.sh: the C
 * library behind it, and no threads, so the bake runs in portBakeTraceStep's
 * time slices on the test's own thread (deterministic, no SDL needed). */
#ifndef TEST_SDL_SHIM_H
#define TEST_SDL_SHIM_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SDLCALL
typedef struct { int value; } SDL_atomic_t;
typedef struct SDL_Thread SDL_Thread;
typedef uint64_t Uint64;
typedef int (*SDL_ThreadFunction)(void *data);

static inline int SDL_AtomicAdd(SDL_atomic_t *a, int v) { const int old = a->value; a->value += v; return old; }
static inline int SDL_AtomicGet(SDL_atomic_t *a) { return a->value; }
static inline int SDL_AtomicSet(SDL_atomic_t *a, int v) { const int old = a->value; a->value = v; return old; }
static inline SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data)
{
    (void) fn; (void) name; (void) data;
    return NULL;
}
static inline void SDL_WaitThread(SDL_Thread *t, int *status) { (void) t; (void) status; }
static inline int SDL_GetCPUCount(void) { return 4; }
static inline Uint64 SDL_GetPerformanceFrequency(void) { return 1000000000ull; }
static inline Uint64 SDL_GetPerformanceCounter(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (Uint64) ts.tv_sec * 1000000000ull + (Uint64) ts.tv_nsec;
}
#define SDL_malloc malloc
#define SDL_free free
#define SDL_qsort qsort
#define SDL_memcpy memcpy
#define SDL_getenv getenv
#define SDL_atoi atoi
#define SDL_sqrtf sqrtf
#define SDL_powf powf
#define SDL_cosf cosf
#define SDL_sinf sinf
#define SDL_fabsf fabsf

#endif
