/* Dress native pistol/chop forearms without changing their bones, grips or
 * animations. Only a scoped first-person draw can reach this replacement. */
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <stdint.h>
#include <string.h>
#include "game/dyn.h"
#include "game/file.h"
#include "game/player.h"
#include "game/othermodemicrocode.h"
#include "gbi_extension.h"
#include "port_wardrobe.h"

static s32 paItem;
static PortWardrobe paStyle;
typedef struct { f32 x[3], uv[2], shade[4]; } ArmVertex;
typedef struct { ArmVertex v[3]; } ArmTriangle;
#define ARM_TRIANGLES 512
#define ARM_VERTICES 6144
/* Scratch is consumed on the game thread; emitted geometry is frame-owned. */
static ArmTriangle paTriangles[ARM_TRIANGLES];
static Vtx paVertices[ARM_VERTICES];

void portWardrobeBegin(s32 item)
{
    paItem = 0;
    switch (item) {
    case ITEM_FIST: case ITEM_WPPK: case ITEM_WPPKSIL: case ITEM_TT33:
    case ITEM_RUGER: case ITEM_GOLDENGUN: case ITEM_SILVERWPPK: case ITEM_GOLDWPPK:
        paItem = item;
        paStyle = portWardrobeStyle(g_CurrentPlayer->bondtype, fileGetBondForCurrentFolder());
        break;
    default: break;
    }
}

void portWardrobeEnd(void) { paItem = 0; }

static ArmVertex paRead(const Vtx *v)
{
    ArmVertex out;
    s32 i;
    for (i = 0; i < 3; i++) out.x[i] = v->v.ob[i];
    for (i = 0; i < 2; i++) out.uv[i] = v->v.tc[i];
    for (i = 0; i < 4; i++) out.shade[i] = v->v.cn[i];
    return out;
}

/* Sutherland-Hodgman, including interpolated native UVs and baked lighting. */
static s32 paClip(const ArmVertex *in, s32 n, ArmVertex *out, f32 z, s32 below)
{
    s32 i, count = 0;
    for (i = 0; i < n; i++) {
        const ArmVertex *a = &in[i], *b = &in[(i+1)%n];
        s32 insideA = below ? a->x[2] <= z : a->x[2] >= z;
        s32 insideB = below ? b->x[2] <= z : b->x[2] >= z;
        if (insideA) out[count++] = *a;
        if (insideA != insideB) {
            f32 t = (z-a->x[2])/(b->x[2]-a->x[2]);
            ArmVertex v;
            s32 k;
            for (k = 0; k < 3; k++) v.x[k] = a->x[k]+t*(b->x[k]-a->x[k]);
            for (k = 0; k < 2; k++) v.uv[k] = a->uv[k]+t*(b->uv[k]-a->uv[k]);
            for (k = 0; k < 4; k++) v.shade[k] = a->shade[k]+t*(b->shade[k]-a->shade[k]);
            out[count++] = v;
        }
    }
    return count;
}

static s32 paTriangle(const Vtx **cache, s32 a, s32 b, s32 c, s32 *count)
{
    if (a == b || b == c || c == a) return TRUE;
    if (a >= 16 || b >= 16 || c >= 16 || !cache[a] || !cache[b] || !cache[c]
        || *count >= ARM_TRIANGLES) return FALSE;
    paTriangles[*count].v[0] = paRead(cache[a]);
    paTriangles[*count].v[1] = paRead(cache[b]);
    paTriangles[(*count)++].v[2] = paRead(cache[c]);
    return TRUE;
}

static s32 paDecode(const ModelRoData_DisplayListRecord *data, u32 *matrix)
{
    const Vtx *cache[16] = {0};
    const Gfx *dl = data->Primary;
    s32 count = 0, i;
    *matrix = 0;
    for (i = 0; i < 4096; i++) {
        u32 w0 = dl[i].words.w0, w1 = dl[i].words.w1;
        u32 op = w0 >> 24;
        if (op == (u8)G_ENDDL) return *matrix ? count : -1;
        if (op == G_DL) return -1; /* Unexpected nested geometry: retain native draw. */
        if (op == G_MTX) {
            if ((w1 >> 24) != 3 || (*matrix && *matrix != w1)) return -1;
            *matrix = w1;
        } else if (op == G_VTX) {
            s32 n = (w0 & 0xffff)/sizeof(Vtx), first = (w0 >> 16)&15, j;
            uintptr_t ptr = (uintptr_t)data->BaseAddr + (w1 & 0xffffff);
            uintptr_t begin = (uintptr_t)data->Vertices;
            if ((w1 >> 24) != 5 || !n || first+n > 16 || ptr < begin
                || ptr+n*sizeof(Vtx) > begin+data->numVertices*sizeof(Vtx)) return -1;
            for (j = 0; j < n; j++) cache[first+j] = (const Vtx *)ptr+j;
        } else if (op == (u8)G_TRI1) {
            if (!paTriangle(cache, ((w1>>16)&255)/10, ((w1>>8)&255)/10, (w1&255)/10, &count)) return -1;
        } else if (op == (u8)G_TRI4) {
            s32 j;
            for (j = 0; j < 4; j++) if (!paTriangle(cache, (w1>>(8*j))&15,
                (w1>>(8*j+4))&15, (w0>>(4*j))&15, &count)) return -1;
        }
    }
    return -1;
}

static void paStore(Vtx *dst, const ArmVertex *v, s32 material, f32 light)
{
    s32 k;
    for (k = 0; k < 3; k++) dst->v.ob[k] = (s16)v->x[k];
    dst->v.flag = 0;
    if (material == PORT_SKIN) {
        for (k = 0; k < 2; k++) dst->v.tc[k] = (s16)v->uv[k];
    } else {
        const sImageTableEntry *tex = &paStyle.material[material];
        /* Planar wrap follows the arm's authored long axis; deterministic and
         * independent of camera, animation frame and player position. */
        dst->v.tc[0] = (s16)((v->x[0]+42)*(tex->width-1)*32/84);
        dst->v.tc[1] = (s16)((v->x[2]+200)*(tex->height-1)*32/250);
    }
    for (k = 0; k < 3; k++) dst->v.cn[k] = (u8)(v->shade[k]*light);
    dst->v.cn[3] = (u8)v->shade[3];
}

s32 portWardrobeRenderNode(ModelRenderData *renderdata, ModelNode *node)
{
    ModelRoData_DisplayListRecord *data = &node->Data->DisplayList;
    f32 minZ = 32767, maxZ = -32768, wrist, sleeve, cuff, light;
    s32 i, m, triangles, used = 0, starts[4];
    u32 matrix;
    Gfx *gdl;
    Vtx *vertices;
    if (!paItem || renderdata->unk18 || !(renderdata->flags&1) || !data->Primary
        || !data->Vertices || !data->numVertices || data->Secondary) return FALSE;
    for (i = 0; i < data->numVertices; i++) {
        f32 z = data->Vertices[i].coord.z;
        if (z < minZ) minZ = z;
        if (z > maxZ) maxZ = z;
    }
    /* Native forearms are the only eligible parts extending behind -150.
     * Distal hands/fingers and all weapon metal remain original. */
    if (minZ >= -150 || maxZ > 50) return FALSE;
    triangles = paDecode(data, &matrix);
    if (triangles <= 0) return FALSE;
    wrist = paItem == ITEM_FIST ? 50.0f : 0.0f;
    sleeve = wrist+(minZ-wrist)*paStyle.sleeveEnd;
    cuff = wrist+(minZ-wrist)*paStyle.cuffEnd;
    light = 0.55f+(g_CurrentPlayer->tileColor.r+g_CurrentPlayer->tileColor.g
        +g_CurrentPlayer->tileColor.b)*(0.45f/(255.0f*3.0f));
    for (m = 0; m < PORT_MATERIALS; m++) {
        starts[m] = used;
        for (i = 0; i < triangles; i++) {
            ArmVertex a[8], b[8];
            s32 n, j, k;
            if (m == PORT_SKIN) n = paClip(paTriangles[i].v, 3, a, cuff, FALSE);
            else if (m == PORT_SLEEVE) n = paClip(paTriangles[i].v, 3, a, sleeve, TRUE);
            else {
                n = paClip(paTriangles[i].v, 3, b, cuff, TRUE);
                n = paClip(b, n, a, sleeve, FALSE);
            }
            for (j = 1; j+1 < n; j++) {
                s32 indices[3] = {0, j, j+1};
                if (used+3 > ARM_VERTICES) return FALSE;
                for (k = 0; k < 3; k++) paStore(&paVertices[used++], &a[indices[k]], m, light);
            }
        }
    }
    starts[3] = used;
    vertices = dynAllocateVertices(used);
    memcpy(vertices, paVertices, used*sizeof(Vtx));
    gdl = renderdata->gdl;
    gDPPipeSync(gdl++);
    gSPMatrix(gdl++, matrix, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPClearGeometryMode(gdl++, G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_FOG);
    gSPSetGeometryMode(gdl++, G_SHADE | G_SHADING_SMOOTH);
    for (m = 0; m < PORT_MATERIALS; m++) {
        sImageTableEntry texture = paStyle.material[m];
        if (starts[m] == starts[m+1]) continue;
        texSelect(&gdl, &texture, 4, 0, 0);
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gSPTexture(gdl++, 0xffff, 0xffff, 0, G_TX_RENDERTILE, G_ON);
        gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
        if (renderdata->zbufferenabled) {
            gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
        } else {
            gDPSetRenderMode(gdl++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
        }
        for (i = starts[m]; i < starts[m+1]; i += 12) {
            s32 n = starts[m+1]-i, j;
            if (n > 12) n = 12;
            gSPVertex(gdl++, osVirtualToPhysical(vertices+i), n, 0);
            for (j = 0; j < n; j += 3) gSP1Triangle(gdl++, j, j+1, j+2, 0);
        }
    }
    gDPPipeSync(gdl++);
    renderdata->gdl = gdl;
    return TRUE;
}
