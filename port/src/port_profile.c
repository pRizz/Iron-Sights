/**
 * port_profile.c - GE_PROFILE=<file>: a sampling profiler built into the game.
 *
 * A sampler thread wakes GE_PROFILE_HZ times a second (default 1000). For each
 * other thread that has used processor time since the last wake, it suspends
 * the thread, reads its program counter and walks its stack, and resumes it.
 * A 32-bit build walks the frame-pointer chain (the tree is built with
 * -fno-omit-frame-pointer for the crash handler). A 64-bit build (WIN64-003)
 * unwinds with RtlVirtualUnwind, since MinGW x64's frame pointer does not
 * chain: it finds each function's unwind entry by a binary search of its
 * module's own exception directory, read from a module list refreshed while no
 * thread is suspended. RtlLookupFunctionEntry would take the loader's
 * function-table lock, which a suspended thread loading a DLL may hold. Samples are kept in memory and written once, by portProfileWrite.
 * scripts/port/bench/bench_profile.py turns the file into a table of functions.
 *
 * A thread that used no processor time is not sampled, so the counts are
 * processor time, not time spent waiting. At 32 bits, code without frame
 * pointers (the GL driver, system DLLs) ends a walk early; the sample still
 * names its module. At 64 bits a walk ends at an address in no module.
 *
 * File: '# ge-profile 1 hz=<n>', one 'M <base> <size> <path>' line per loaded
 * module, then one line per sample:
 * 'S <thread id> <stage> <tick> <wake> <microseconds> <pc> <caller>...', where
 * wake counts the sampler's wake-ups, so samples over wake-ups is a thread's
 * share of one processor. Addresses are hex. At 64 bits modules load where
 * ASLR puts them: bench_profile.py rebases each address from its module's M
 * line to the executable's preferred base.
 *
 * Windows only. It starts itself from a constructor, so no other file needs
 * to call it, and the stock comparison build compiles this same file
 * (scripts/port/bench/stock.py).
 *
 * The width is told by _WIN64, not IS_64_BIT, since stock trees lack
 * platform_info.h. At 32 bits a sample word is 32 bits, as it always was.
 */
#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>

#define PROFILE_MAX_THREADS 64
#define PROFILE_MAX_DEPTH 48
#define PROFILE_WORDS (12u * 1024u * 1024u) /* 48 MB of samples, 96 MB at 64 bits */
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

#ifdef _WIN64
typedef unsigned long long ProfileWord; /* holds an address */
#define PROFILE_WORD_FMT " %llx"
#define PROFILE_MODULE_FMT "M %llx %llx %s\n"
typedef unsigned long long ProfileModuleWord;
#define PROFILE_MAX_MODULES 256

/* A loaded module and its exception directory (its functions' unwind
 * entries, sorted by address), for the 64-bit walk. */
typedef struct ProfileModule
{
    ULONG_PTR base;
    ULONG_PTR end;
    const RUNTIME_FUNCTION *functions;
    DWORD count;
} ProfileModule;

static ProfileModule sModules[PROFILE_MAX_MODULES];
static int sModuleCount;
#else
typedef unsigned int ProfileWord;
#define PROFILE_WORD_FMT " %x"
#define PROFILE_MODULE_FMT "M %lx %lx %s\n"
typedef unsigned long ProfileModuleWord;
#endif

static const char *sPath;
static unsigned int sHz = 1000;
static ProfileWord *sWords;
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

#ifdef _WIN64
/* The loaded modules and their exception directories. Called by the sampler
 * while no thread is suspended, so the snapshot cannot wait on a lock a
 * suspended thread holds. */
static void profileRefreshModules(void)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    MODULEENTRY32 module;
    int count = 0;

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }
    module.dwSize = sizeof(module);
    if (Module32First(snapshot, &module))
    {
        do
        {
            const BYTE *base = module.modBaseAddr;
            const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *) base;
            const IMAGE_NT_HEADERS64 *nt;
            const IMAGE_DATA_DIRECTORY *directory;

            if (count == PROFILE_MAX_MODULES || dos->e_magic != IMAGE_DOS_SIGNATURE)
            {
                continue;
            }
            nt = (const IMAGE_NT_HEADERS64 *) (base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                continue;
            }
            directory = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
            sModules[count].base = (ULONG_PTR) base;
            sModules[count].end = (ULONG_PTR) base + module.modBaseSize;
            sModules[count].functions = (const RUNTIME_FUNCTION *) (base + directory->VirtualAddress);
            sModules[count].count = directory->VirtualAddress != 0 ? directory->Size / sizeof(RUNTIME_FUNCTION) : 0;
            count++;
        } while (Module32Next(snapshot, &module));
    }
    CloseHandle(snapshot);
    sModuleCount = count;
}

/* The module holding ADDRESS, or NULL. */
static const ProfileModule *profileModuleOf(ULONG_PTR address)
{
    int i;

    for (i = 0; i < sModuleCount; i++)
    {
        if (sModules[i].base <= address && address < sModules[i].end)
        {
            return &sModules[i];
        }
    }
    return NULL;
}

/* MODULE's unwind entry for the function holding ADDRESS, or NULL for a leaf
 * function, which has none. */
static PRUNTIME_FUNCTION profileFunctionOf(const ProfileModule *module, ULONG_PTR address)
{
    const DWORD rva = (DWORD) (address - module->base);
    DWORD low = 0, high = module->count;

    while (low < high)
    {
        const DWORD middle = low + (high - low) / 2;
        const RUNTIME_FUNCTION *entry = &module->functions[middle];

        if (rva < entry->BeginAddress)
        {
            high = middle;
        }
        else if (rva >= entry->EndAddress)
        {
            low = middle + 1;
        }
        else
        {
            return (PRUNTIME_FUNCTION) entry;
        }
    }
    return NULL;
}

/* Writes CONTEXT's program counter and its callers' return addresses to OUT,
 * innermost first; returns how many. It stops at an address in no module, at
 * a stack pointer that does not rise, and at the stack's committed end. */
static int profileUnwind(CONTEXT *context, ProfileWord *out)
{
    MEMORY_BASIC_INFORMATION region;
    ULONG_PTR limit = 0;
    int depth = 1;

    out[0] = context->Rip;
    if (VirtualQuery((LPCVOID) context->Rsp, &region, sizeof(region)) != 0)
    {
        limit = (ULONG_PTR) region.BaseAddress + region.RegionSize;
    }
    while (depth < PROFILE_MAX_DEPTH)
    {
        const ProfileModule *module = profileModuleOf(context->Rip);
        const DWORD64 before = context->Rsp;
        PRUNTIME_FUNCTION function;

        if (module == NULL)
        {
            break;
        }
        function = profileFunctionOf(module, context->Rip);
        if (function != NULL)
        {
            PVOID handlerData;
            DWORD64 establisher;

            RtlVirtualUnwind(UNW_FLAG_NHANDLER, module->base, context->Rip, function, context, &handlerData,
                             &establisher, NULL);
        }
        else
        {
            /* A leaf: its return address is on top of the stack. */
            if (context->Rsp + sizeof(DWORD64) > limit)
            {
                break;
            }
            context->Rip = *(const DWORD64 *) context->Rsp;
            context->Rsp += sizeof(DWORD64);
        }
        if (context->Rip == 0 || context->Rsp <= before || context->Rsp >= limit)
        {
            break;
        }
        out[depth++] = context->Rip;
    }
    return depth;
}
#endif

/* One sample of a suspended thread. Nothing here may allocate or print: the
 * suspended thread may hold the heap or stdio lock. */
static void profileSample(const ProfileThread *thread)
{
    extern int g_GlobalTickCount; /* s32 */
    extern int g_StageNum;        /* s32 */
    CONTEXT context;
#ifndef _WIN64
    MEMORY_BASIC_INFORMATION region;
    unsigned long frame, limit;
#endif
    ProfileWord *out;
    LONG at = sCount;
    int depth = 1;

    if ((unsigned int) at + PROFILE_MAX_DEPTH + PROFILE_HEADER >= PROFILE_WORDS)
    {
        sStop = 1;
        return;
    }
#ifdef _WIN64
    context.ContextFlags = CONTEXT_FULL; /* the unwind restores the saved integer registers */
#else
    context.ContextFlags = CONTEXT_CONTROL;
#endif
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
#ifdef _WIN64
    depth = profileUnwind(&context, &out[PROFILE_HEADER]);
#else
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
#endif
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
#ifdef _WIN64
            profileRefreshModules();
#endif
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
                fprintf(maybeFile, PROFILE_MODULE_FMT, (ProfileModuleWord) module.modBaseAddr,
                        (ProfileModuleWord) module.modBaseSize, module.szExePath);
            } while (Module32Next(snapshot, &module));
        }
        CloseHandle(snapshot);
    }
    for (at = 0; at < count; at += PROFILE_HEADER + (LONG) sWords[at + 5])
    {
        unsigned int i;

        fprintf(maybeFile, "S %u %d %d %u %u", (unsigned int) sWords[at], (int) sWords[at + 1],
                (int) sWords[at + 2], (unsigned int) sWords[at + 3], (unsigned int) sWords[at + 4]);
        for (i = 0; i < sWords[at + 5]; i++)
        {
            fprintf(maybeFile, PROFILE_WORD_FMT, sWords[at + PROFILE_HEADER + i]);
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
    sWords = (ProfileWord *) VirtualAlloc(NULL, PROFILE_WORDS * sizeof(ProfileWord), MEM_RESERVE | MEM_COMMIT,
                                          PAGE_READWRITE);
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
