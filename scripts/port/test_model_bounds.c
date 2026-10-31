#include "port/src/romfile_model.c"
#include "test_buffer_helpers.h"

static void truncated_records(void)
{
    const u8 types[] = {MODELNODE_OPCODE_DL, MODELNODE_OPCODE_DLCOLLISION,
        MODELNODE_OPCODE_DLPRIMARY, MODELNODE_OPCODE_LOD, MODELNODE_OPCODE_BSP, MODELNODE_OPCODE_SWITCH};
    for (u32 i = 0; i < sizeof(types); i++) {
        /* Arrange: a valid node pointing at one remaining byte of rodata. */
        u8 *data = calloc(1, 48);
        data[1] = types[i]; data[4] = 5; data[7] = 47;
        /* Act/assert: reject before reading any record's fields. */
        assert(!romfileModelConvert(data, 48, 0, 0, "truncated", NULL));
        free(data);
    }
}

int main(void)
{
    truncated_records();
    /* Arrange/act/assert: counts and spans cannot wrap around the file size. */
    u8 data[48] = {0}, done[48] = {0};
    struct mconv c = {.d=data, .size=48, .done=done};
    assert(!mark(&c, 0xfffffffc, 8));
    assert(!romfileModelConvert(data, sizeof(data), 0x40000000, 0, "count", NULL));
    assert(!romfileModelConvert(data, sizeof(data), 0, 0x40000000, "count", NULL));
    assert(!romfileModelConvert(NULL, sizeof(data), 0, 0, "null", NULL));

    /* Act/assert: a deterministic malformed-input campaign under sanitizers. */
    u32 random = 1;
    for (u32 round = 0; round < 10000; round++) {
        const u32 size = 24 + round % 488;
        u8 *bytes = malloc(size);
        for (u32 i = 0; i < size; i++) { random = random * 1664525u + 1013904223u; bytes[i] = random >> 24; }
        romfileModelConvert(bytes, size, round % 8, round % 8, "mutated", NULL);
        free(bytes);
    }
    puts("Model span/count checks and 10000 malformed inputs passed");
    return 0;
}
