/* ROM-free stage-reset regression: independent objects must retain pristine IDs. */
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>

extern Gfx globalDL_0x000[];
extern sImageTableEntry s_genericimage[];
extern sImageTableEntry s_mpstageselimages[];
extern void portResetImageBank(void);
extern void *memset(void *, int, size_t);
extern int memcmp(const void *, const void *, size_t);

static void check(s32 condition) { if (!condition) abort(); }

int main(void)
{
    /* Arrange: first, middle and last objects in the bank. */
    Gfx first = globalDL_0x000[0];
    sImageTableEntry middle = s_genericimage[0];
    sImageTableEntry last = s_mpstageselimages[0];
    portResetImageBank();
    memset(&globalDL_0x000[0], 0x31, sizeof(first));
    memset(&s_genericimage[0], 0x62, sizeof(middle));
    memset(&s_mpstageselimages[0], 0x93, sizeof(last));

    /* Act: stage reload restores each object, regardless of link order. */
    portResetImageBank();

    /* Assert: the initial snapshot survives repeated resets. */
    check(memcmp(&globalDL_0x000[0], &first, sizeof(first)) == 0);
    check(memcmp(&s_genericimage[0], &middle, sizeof(middle)) == 0);
    check(memcmp(&s_mpstageselimages[0], &last, sizeof(last)) == 0);
    memset(&s_genericimage[0], 0x55, sizeof(middle));
    portResetImageBank();
    check(memcmp(&s_genericimage[0], &middle, sizeof(middle)) == 0);
    return 0;
}
