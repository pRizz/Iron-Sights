#include "src/token.c"
#include "test_buffer_helpers.h"
int main(void)
{
    /* Arrange: longer text and more tokens than either destination can hold. */
    char text[4096];
    for (u32 i=0; i<sizeof(text)-1; i++) text[i] = i&1 ? ' ' : 'x';
    text[sizeof(text)-1] = 0;
    /* Act/assert: both buffers remain terminated and bounded. */
    tokenSetString(text);
    assert(g_TokenCount <= sizeof(g_Tokens)/sizeof(g_Tokens[0]));
    assert(((char *)g_TokenString)[sizeof(g_TokenString)-1] == 0);
    tokenSetString("   "); assert(g_TokenCount == 1);
    tokenSetString("-mgfx60 -mvtx40"); assert(g_TokenCount == 3);
    puts("Token string/table capacity checks passed");
    return 0;
}
