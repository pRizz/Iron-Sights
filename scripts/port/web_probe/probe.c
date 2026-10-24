/* ROM-free ABI and address-range experiments; no game/runtime implementation. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <emscripten/heap.h>
#include <ultra64.h>
#include <PR/abi.h>
#include "ob.h"
#include "port_hostptr.h"

_Static_assert(sizeof(void *) == 4, "The engine requires 32-bit pointers");
_Static_assert(sizeof(uintptr_t) == 4, "Display lists store pointer words");
_Static_assert(sizeof(long) == 4, "N64-compatible long width");
_Static_assert(sizeof(Gfx) == 8, "Display-list command stride");
_Static_assert(offsetof(Gfx, words.w1) == 4, "Display-list address word");
_Static_assert(sizeof(Vtx) == 16, "Native vertex stride");
_Static_assert(sizeof(Mtx) == 64, "Native matrix layout");
_Static_assert(sizeof(Vp) == 16, "Native viewport layout");
_Static_assert(sizeof(Acmd) == 8, "Audio command stride");
_Static_assert(sizeof(fileentry) == 12, "ROM file table pointer layout");
_Static_assert(sizeof(resource_lookup_data_entry) == 20, "ROM lookup layout");

#define MIB (1024u * 1024u)
static u8 static_bytes[16];

/* The repository's assert.h logs without aborting; checks must fail the run. */
static void require(int condition, const char *expression)
{
    if (condition) return;
    fprintf(stderr, "Probe check failed: %s\n", expression);
    exit(2);
}
#define CHECK(condition) require((condition), #condition)

static int callback(int value)
{
    return value + 7;
}

static u8 *allocate(u32 bytes)
{
    u8 *maybe_data = calloc(1, bytes);
    CHECK(maybe_data != NULL);
    maybe_data[bytes - 1] = 42;
    return maybe_data;
}

static void check_abi(void)
{
    u32 word = 0x04030201;
    volatile f32 rounded = 16777216.0f;
    int (*function)(int) = callback;
    uintptr_t stored_function = (uintptr_t) function;
    int (*restored)(int) = (int (*)(int)) stored_function;
    Gfx command;

    CHECK(*(u8 *) &word == 1);
    CHECK((char) 0xff == 255);
    CHECK(rounded + 1.0f == rounded);
    CHECK(sizeof(function) == sizeof(u32));
    CHECK(restored(5) == 12);
    command.words.w0 = 0;
    command.words.w1 = (u32) static_bytes;
    CHECK((void *) command.words.w1 == static_bytes);
}

int main(void)
{
    const size_t initial = emscripten_get_heap_size();
    u8 *arena = allocate(64 * MIB);
    u8 *converted = allocate(64 * MIB);
    u8 *rom = allocate(12 * MIB);
    const uintptr_t arena_start = (uintptr_t) arena;
    const uintptr_t arena_end = arena_start + 64 * MIB;
    const uintptr_t segment_word = 0x03000120u;
    const int collision = arena_start <= segment_word && segment_word < arena_end;
    const int static_native = PORT_IS_HOST_ADDR(static_bytes);
    const int arena_native = PORT_IS_HOST_ADDR(arena);
    const int rom_native = PORT_IS_HOST_ADDR(rom);

    check_abi();
    CHECK(static_native == PROBE_HIGH);
    CHECK(arena_native == PROBE_HIGH);
    CHECK(rom_native == PROBE_HIGH);
    CHECK(collision == !PROBE_HIGH);
    CHECK(!PORT_IS_HOST_ADDR(segment_word));
    printf("{\"layout\":\"%s\",\"abi_passed\":true,"
        "\"initial_linear_bytes\":%zu,\"final_linear_bytes\":%zu,"
        "\"requested_arena_bytes\":%u,\"static_address\":%u,"
        "\"arena_address\":%u,\"converted_address\":%u,\"rom_address\":%u,"
        "\"native_classifier_accepts_host\":%s,\"segment_word_overlaps_arena\":%s}\n",
        PROBE_HIGH ? "high" : "compact", initial, emscripten_get_heap_size(),
        140 * MIB, (u32) static_bytes, (u32) arena, (u32) converted, (u32) rom,
        static_native && arena_native && rom_native ? "true" : "false",
        collision ? "true" : "false");
    free(rom);
    free(converted);
    free(arena);
    return 0;
}
