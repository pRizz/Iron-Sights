/* Native geometry and wardrobe checks without ROM assets or a renderer. */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../../port/src/port_wardrobe.c"
#include "../../port/src/port_native_arms.c"
static struct player player;
struct player *g_CurrentPlayer = &player;
static Vtx allocated[ARM_VERTICES];
static s32 allocatedCount, bound[3], boundCount;
void osSyncPrintf(const char *fmt, ...) {
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args); abort();
}
Vtx *dynAllocateVertices(s32 count) { assert(count <= ARM_VERTICES); allocatedCount = count; return allocated; }
u32 osVirtualToPhysical(void *p) { return (u32)p; }
s32 fileGetBondForCurrentFolder(void) { return BOND_CONNERY; }
void texSelect(Gfx **gdl, sImageTableEntry *texture, u32 a, s32 b, u32 c)
{ assert(boundCount < 3); bound[boundCount++] = texture->index; }
int main(void)
{
    Vtx source[3] = {{{{-20,-20,-190},0,{0,0},{255,255,255,255}}},
                     {{{20,20,50},0,{512,512},{180,180,180,255}}},
                     {{{-20,20,50},0,{0,512},{220,220,220,255}}}};
    Vtx original[3];
    Gfx dl[4], output[2048];
    union ModelRoData data = {0};
    ModelNode node = {0};
    ModelRenderData render = {0};
    s32 variant, i;
    u32 matrix;
    memcpy(original, source, sizeof(source));
    data.DisplayList.Primary = dl;
    data.DisplayList.Vertices = (Vertex *)source;
    data.DisplayList.BaseAddr = source;
    data.DisplayList.numVertices = 3;
    node.Data = &data;
    gSPMatrix(&dl[0], 0x03000040, G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPVertex(&dl[1], 0x05000000, 3, 0);
    gSP1Triangle(&dl[2], 0,1,2,0);
    gSPEndDisplayList(&dl[3]);
    assert(paDecode(&data.DisplayList, &matrix) == 1 && matrix == 0x03000040);
    /* Never read vertices outside the node's source array. */
    dl[1].words.w1 += 16;
    assert(paDecode(&data.DisplayList, &matrix) == -1);
    dl[1].words.w1 -= 16;
    for (variant = 0; variant < 6; variant++) {
        PortWardrobe style = portWardrobeStyle(portWardrobeCuff(variant), BOND_CONNERY);
        assert(portWardrobeVariant(portWardrobeCuff(variant), BOND_CONNERY) == variant);
        player.bondtype = portWardrobeCuff(variant);
        player.tileColor.r = player.tileColor.g = player.tileColor.b = 255;
        boundCount = allocatedCount = 0;
        render.flags = 1; render.gdl = output;
        portWardrobeBegin(ITEM_FIST);
        assert(portWardrobeRenderNode(&render, &node));
        assert(allocatedCount > 3 && allocatedCount%3 == 0 && boundCount == 3);
        assert(bound[PORT_SLEEVE] == style.material[PORT_SLEEVE].index);
        assert(render.gdl > output && render.gdl < output+2048);
        for (i = 0; i < allocatedCount; i++) {
            assert(allocated[i].v.ob[2] >= -190 && allocated[i].v.ob[2] <= 50);
            assert(allocated[i].v.cn[3] == 255);
        }
        assert(memcmp(source, original, sizeof(source)) == 0);
        portWardrobeEnd();
        assert(!portWardrobeRenderNode(&render, &node));
    }
    assert(portWardrobeVariant(CUFF_FOLDER, BOND_CONNERY) == 5);
    assert(portWardrobeVariant(CUFF_FOLDER, BOND_BROSNAN) == 4);
    portWardrobeBegin(ITEM_AK47); assert(!paItem);
    for (i = ITEM_WPPK; i <= ITEM_TT33; i++) { portWardrobeBegin(i); assert(paItem == i); }
    portWardrobeBegin(ITEM_RUGER); assert(paItem == ITEM_RUGER);
    portWardrobeBegin(ITEM_GOLDENGUN); assert(paItem == ITEM_GOLDENGUN);
    portWardrobeBegin(ITEM_SILVERWPPK); assert(paItem == ITEM_SILVERWPPK);
    portWardrobeBegin(ITEM_GOLDWPPK); assert(paItem == ITEM_GOLDWPPK);
    puts("Wardrobe checks passed: six outfits, folder resolution, native bones/UVs, clipping bounds, immutable source and pistol/chop scope.");
    return 0;
}
