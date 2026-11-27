/**
 * port_win_context.h - a Windows thread context at either width (WIN64-001,
 * scripts/port/windows_x64_win64_001.md).
 *
 * The crash handler, first-chance reporter, stall watchdog and watchpoints in
 * port/src/main.c read the program counter, stack and frame registers, walk
 * frame-pointer chains and parse PE headers. Those names and sizes differ
 * between x86 and x64; everything else in that code does not. These macros
 * name them, so one copy of the code serves both widths. At 32 bits each one
 * expands to exactly what main.c spelled before (Eip, unsigned long, "%08lx",
 * IMAGE_FILE_MACHINE_I386, the PE32 offsets), so the 32-bit executable's code
 * is unchanged; compare_win32.py checks that.
 *
 * Tests _WIN64, the compiler's own definition, rather than IS_64_BIT: this is
 * Windows-only code, and the two agree on every Windows build.
 */
#ifndef PORT_WIN_CONTEXT_H
#define PORT_WIN_CONTEXT_H

#ifdef _WIN32
#include <windows.h>

#ifdef _WIN64
/* A code or stack address, as the frame walks read it from a stack slot. */
typedef unsigned long long PortWinAddr;
#define PORT_WIN_ADDR_FMT "%016llx"
#define PORT_WIN_HEX_FMT "%llx" /* an offset into a module, unpadded */
#define PORT_WIN_STRTOADDR strtoull
#define PORT_WIN_PC(context) ((context)->Rip)
#define PORT_WIN_SP(context) ((context)->Rsp)
#define PORT_WIN_FP(context) ((context)->Rbp)
/* A debug register's type in CONTEXT (Dr0 to Dr3). */
typedef DWORD64 PortWinDebugReg;
#define PORT_WIN_MACHINE IMAGE_FILE_MACHINE_AMD64
/* The optional header's magic, and the offset from the "PE\0\0" signature to
 * DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]: 4 + 20 (COFF) + 0x70. */
#define PORT_WIN_PE_MAGIC 0x20B
#define PORT_WIN_PE_EXPORT_DIR 0x88
#else
typedef unsigned long PortWinAddr;
#define PORT_WIN_ADDR_FMT "%08lx"
#define PORT_WIN_HEX_FMT "%lx"
#define PORT_WIN_STRTOADDR strtoul
#define PORT_WIN_PC(context) ((context)->Eip)
#define PORT_WIN_SP(context) ((context)->Esp)
#define PORT_WIN_FP(context) ((context)->Ebp)
typedef DWORD PortWinDebugReg;
#define PORT_WIN_MACHINE IMAGE_FILE_MACHINE_I386
/* 4 + 20 (COFF) + 0x60 */
#define PORT_WIN_PE_MAGIC 0x10B
#define PORT_WIN_PE_EXPORT_DIR 0x78
#endif

#endif /* _WIN32 */
#endif /* PORT_WIN_CONTEXT_H */
