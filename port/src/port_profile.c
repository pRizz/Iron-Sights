/**
 * port_profile.c - GE_PROFILE=<file>: a sampling profiler built into the game.
 *
 * A sampler thread wakes GE_PROFILE_HZ times a second (default 1000). For each
 * other thread that has used processor time since the last wake, it suspends
 * the thread, reads its program counter and walks its frame-pointer chain (the
 * tree is built with -fno-omit-frame-pointer for the crash handler), and
 * resumes it. Samples are kept in memory and written once, by portProfileWrite.
 * scripts/port/bench/bench_profile.py turns the file into a table of functions.
 *
 * A thread that used no processor time is not sampled, so the counts are
 * processor time, not time spent waiting. Code without frame pointers (the GL
 * driver, system DLLs) ends a walk early; the sample still names its module.
 *
 * File: '# ge-profile 1 hz=<n>', one 'M <base> <size> <path>' line per loaded
 * module, then one line per sample:
 * 'S <thread id> <stage> <tick> <wake> <microseconds> <pc> <caller>...', where
 * wake counts the sampler's wake-ups, so samples over wake-ups is a thread's
 * share of one processor.
 *
 * Windows only. It starts itself from a constructor, so no other file needs
 * to call it, and the stock comparison build compiles this same file
 * (scripts/port/bench/stock.py).
 */
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>

#define PROFILE_MAX_THREADS 64
#define PROFILE_MAX_DEPTH 48
#define PROFILE_WORDS (12u * 1024u * 1024u) /* 48 MB of samples */
#define PROFILE_REFRESH_TICKS 250           /* look for new threads this often */
/* Processor cycles a thread must have used since its last sample to count as
 * having run: a waiting thread still ticks a little when it is woken. */
#define PROFILE_RAN_CYCLES 20000ull

typedef struct ProfileThread
{
    DWORD id;
    HANDLE handle;
    ULONG64 cycles;
} ProfileThread;

static const char *sPath;
static unsigned int sHz = 1000;
static unsigned int *sWords;
static volatile LONG sCount;
static volatile LONG sStop;
static ProfileThread sThreads[PROFILE_MAX_THREADS];
static int sThreadCount;
static DWORD sSamplerId;
static unsigned int sWake;
static LARGE_INTEGER sClockHz, sClockStart;

#define PROFILE_HEADER 6 /* words before the addresses of a sample */

static void profileRefreshThreads(void)
{
    const DWORD process = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 entry;

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry))
    {
        do
        {
            int i, known = 0;

            if (entry.th32OwnerProcessID != process || entry.th32ThreadID == sSamplerId)
            {
                continue;
            }
            for (i = 0; i < sThreadCount; i++)
            {
                known |= (sThreads[i].id == entry.th32ThreadID);
            }
            if (!known && sThreadCount < PROFILE_MAX_THREADS)
            {
                HANDLE handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                                           FALSE, entry.th32ThreadID);

                if (handle != NULL)
                {
                    sThreads[sThreadCount].id = entry.th32ThreadID;
                    sThreads[sThreadCount].handle = handle;
                    QueryThreadCycleTime(handle, &sThreads[sThreadCount].cycles);
                    sThreadCount++;
                }
            }
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

/* One sample of a suspended thread. Nothing here may allocate or print: the
 * suspended thread may hold the heap or stdio lock. */
static void profileSample(const ProfileThread *thread)
{
    extern int g_GlobalTickCount; /* s32 */
    extern int g_StageNum;        /* s32 */
    CONTEXT context;
    MEMORY_BASIC_INFORMATION region;
    unsigned int *out;
    unsigned long frame, limit;
    LONG at = sCount;
    int depth = 1;

    if ((unsigned int) at + PROFILE_MAX_DEPTH + PROFILE_HEADER >= PROFILE_WORDS)
    {
        sStop = 1;
        return;
    }
    context.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(thread->handle, &context))
    {
        return;
    }
    out = &sWords[at];
    out[0] = thread->id;
    out[1] = (unsigned int) g_StageNum;
    out[2] = (unsigned int) g_GlobalTickCount;
    out[3] = sWake;
    {
        LARGE_INTEGER now;

        QueryPerformanceCounter(&now);
        out[4] = (unsigned int) ((now.QuadPart - sClockStart.QuadPart) * 1000000 / sClockHz.QuadPart);
    }
    out[PROFILE_HEADER] = context.Eip;

    /* The stack from esp up to the end of its committed region is readable. */
    limit = 0;
    if (VirtualQuery((LPCVOID) context.Esp, &region, sizeof(region)) != 0)
    {
        limit = (unsigned long) region.BaseAddress + region.RegionSize;
    }
    frame = context.Ebp;
    while (depth < PROFILE_MAX_DEPTH && frame >= context.Esp && frame + 8 <= limit && (frame & 3) == 0)
    {
        const unsigned long *fp = (const unsigned long *) frame;

        if (fp[1] == 0)
        {
            break;
        }
        out[PROFILE_HEADER + depth++] = fp[1];
        if (fp[0] <= frame)
        {
            break; /* the stack grows down; a caller's frame must be higher */
        }
        frame = fp[0];
    }
    out[5] = (unsigned int) depth;
    sCount = at + PROFILE_HEADER + depth;
}

static DWORD WINAPI profileThread(LPVOID unused)
{
    HANDLE timer = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /* high resolution */, TIMER_ALL_ACCESS);
    unsigned int tick = 0;

    (void) unused;
    sSamplerId = GetCurrentThreadId();
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while (!sStop)
    {
        int i;

        if (timer != NULL)
        {
            LARGE_INTEGER due;

            due.QuadPart = -(LONGLONG) (10000000u / sHz);
            SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
            WaitForSingleObject(timer, INFINITE);
        }
        else
        {
            Sleep(1);
        }
        sWake++;
        if ((tick++ % PROFILE_REFRESH_TICKS) == 0)
        {
            profileRefreshThreads();
        }
        for (i = 0; i < sThreadCount && !sStop; i++)
        {
            ULONG64 cycles = 0;

            QueryThreadCycleTime(sThreads[i].handle, &cycles);
            if (cycles - sThreads[i].cycles < PROFILE_RAN_CYCLES)
            {
                continue; /* it has not run: nothing to charge */
            }
            if (SuspendThread(sThreads[i].handle) != (DWORD) -1)
            {
                profileSample(&sThreads[i]);
                ResumeThread(sThreads[i].handle);
            }
            /* Read again: being suspended and resumed costs the thread cycles
             * of its own, which must not make it look busy next time. */
            QueryThreadCycleTime(sThreads[i].handle, &sThreads[i].cycles);
        }
    }
    return 0;
}

/** Write the samples. port_bench.c calls this before it leaves the process. */
void portProfileWrite(void)
{
    FILE *maybeFile;
    HANDLE snapshot;
    LONG count, at;

    if (sWords == NULL || sPath == NULL)
    {
        return;
    }
    sStop = 1;
    Sleep(20); /* let a sample in progress finish and resume its thread */
    count = sCount;
    maybeFile = fopen(sPath, "w");
    sPath = NULL;
    if (maybeFile == NULL)
    {
        return;
    }
    fprintf(maybeFile, "# ge-profile 1 hz=%u\n", sHz);
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32 module;

        module.dwSize = sizeof(module);
        if (Module32First(snapshot, &module))
        {
            do
            {
                fprintf(maybeFile, "M %lx %lx %s\n", (unsigned long) module.modBaseAddr,
                        (unsigned long) module.modBaseSize, module.szExePath);
            } while (Module32Next(snapshot, &module));
        }
        CloseHandle(snapshot);
    }
    for (at = 0; at < count; at += PROFILE_HEADER + (LONG) sWords[at + 5])
    {
        unsigned int i;

        fprintf(maybeFile, "S %u %d %d %u %u", sWords[at], (int) sWords[at + 1], (int) sWords[at + 2],
                sWords[at + 3], sWords[at + 4]);
        for (i = 0; i < sWords[at + 5]; i++)
        {
            fprintf(maybeFile, " %x", sWords[at + PROFILE_HEADER + i]);
        }
        fputc('\n', maybeFile);
    }
    fclose(maybeFile);
    printf("ge: profile: wrote %ld words of samples\n", (long) count);
    fflush(stdout);
}

__attribute__((constructor)) static void profileStart(void)
{
    const char *maybePath = getenv("GE_PROFILE");
    const char *maybeHz = getenv("GE_PROFILE_HZ");

    if (maybePath == NULL || maybePath[0] == '\0')
    {
        return;
    }
    if (maybeHz != NULL && atoi(maybeHz) >= 10 && atoi(maybeHz) <= 8000)
    {
        sHz = (unsigned int) atoi(maybeHz);
    }
    sWords = (unsigned int *) VirtualAlloc(NULL, PROFILE_WORDS * sizeof(unsigned int),
                                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (sWords == NULL)
    {
        return;
    }
    sPath = maybePath;
    QueryPerformanceFrequency(&sClockHz);
    QueryPerformanceCounter(&sClockStart);
    CloseHandle(CreateThread(NULL, 0, profileThread, NULL, 0, NULL));
}
#else
void portProfileWrite(void)
{
}
#endif
