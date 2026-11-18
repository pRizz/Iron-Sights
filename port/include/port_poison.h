/**
 * Manual poisoning of the game's own arenas (LP64-005 Task 2).
 *
 * The stage bank (memp), the room heap (mema), the graphics pool (dyn) and
 * the music heap are each one allocation that the game carves into blocks,
 * so AddressSanitizer cannot see one block run into the next. In a build
 * whose compiler defines __SANITIZE_ADDRESS__ each allocator poisons what it
 * has not handed out and unpoisons exactly the bytes a caller asked for;
 * a touch of a poisoned byte is reported as use-after-poison.
 *
 * Without the sanitizer both macros are ((void) 0) and PORT_POISON_GAP is 0:
 * no shipped build changes, and every allocator places its blocks as before.
 *
 * PORT_POISON_GAP is the poisoned run the stage bank leaves after each block
 * in a sanitizer build, so an overrun of the bank's last byte lands on poison
 * and not on the next caller's block. A sanitizer build's bank layout differs
 * from a plain build's on purpose.
 */
#ifndef PORT_POISON_H
#define PORT_POISON_H

#if defined(__SANITIZE_ADDRESS__)

void __asan_poison_memory_region(void const volatile *address, __SIZE_TYPE__ bytes);
void __asan_unpoison_memory_region(void const volatile *address, __SIZE_TYPE__ bytes);

#define PORT_POISON(address, bytes) __asan_poison_memory_region((address), (__SIZE_TYPE__)(bytes))
#define PORT_UNPOISON(address, bytes) __asan_unpoison_memory_region((address), (__SIZE_TYPE__)(bytes))
#ifndef PORT_POISON_GAP /* test_memory_banks.c pins the plain layout with 0 */
#define PORT_POISON_GAP 32
#endif

#else

#define PORT_POISON(address, bytes) ((void) 0)
#define PORT_UNPOISON(address, bytes) ((void) 0)
#undef PORT_POISON_GAP
#define PORT_POISON_GAP 0

#endif

#endif
