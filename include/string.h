#if !defined(TARGET_N64) && defined(__cplusplus)
/* PC port: this header shadows the toolchain's <string.h>; C++ TUs (fast3d)
 * defer to the real one, which libstdc++ needs. C TUs keep the minimal stub,
 * because the hosted header's bcopy/bzero/bcmp conflict with PR/os.h. */
#include_next <string.h>
#else

#ifndef _STRING_H_
#define _STRING_H_
#include <PR/ultratypes.h>

extern void *memcpy(void *, const void *, size_t);
extern unsigned char *strchr(const unsigned char *, int);
extern size_t strlen(const unsigned char *);

#ifndef TARGET_N64
/* PC port: hosted C library functions the port's C code calls. Without a
 * prototype their pointer results would be implicit ints (LP64-002). */
extern void *memset(void *, int, size_t);
extern void *memmove(void *, const void *, size_t);
extern void *memchr(const void *, int, size_t);
extern char *strrchr(const char *, int);
extern char *strstr(const char *, const char *);
extern char *strtok(char *, const char *);
extern char *strerror(int);
extern int memcmp(const void *, const void *, size_t);
extern size_t strspn(const char *, const char *);
extern size_t strcspn(const char *, const char *);
/* Prototyped through other headers in the Linux trees; the MinGW x64 gate
 * tree reaches these calls without one (WIN64-001). */
extern int strcmp(const char *, const char *);
#ifdef _WIN32
extern int _stricmp(const char *, const char *); /* the C runtime's, for main.c */
#endif
#endif

#endif
#endif /* TARGET_N64 */
