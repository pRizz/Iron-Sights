#include "port/src/pcsched.c"
#include "test_buffer_helpers.h"

int main(void)
{
    /* Oversized alignment must fail before attempting an OS mapping. */
    assert(portAllocHigh(0xffffffffu) == NULL);
    assert(portAllocHigh(0xfffffff1u) == NULL);
    assert(portAllocHighDedicated(0xffffffffu) == NULL);
    assert(portAllocHighDedicated(0xffff0001u) == NULL);
    puts("High-address allocator alignment checks passed");
    return 0;
}
