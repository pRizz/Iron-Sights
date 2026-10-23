#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "port_ads.h"
#include "game/dyn.h"
#include "game/player.h"
#include "game/bondview.h"
#include "game/matrixmath.h"
#include "game/file.h"
#include "game/othermodemicrocode.h"
#include "gbi_extension.h"

#include "port_wardrobe.h"
#define ADS_SKIN PORT_SKIN
#define ADS_SLEEVE PORT_SLEEVE
#define ADS_CUFF PORT_CUFF
#define ADS_MATERIALS PORT_MATERIALS
typedef PortWardrobe AdsWardrobe;
#define adsHandWardrobe portWardrobeStyle

static coord3d adsHandLerp(coord3d a, coord3d b, f32 t)
{
    coord3d p = {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t};
    return p;
}

/* Grip centres in weapon-local view units (the original models use ten times these). */
static const struct {
    s32 item;
    coord3d trigger;
    coord3d support;
} sAdsGrips[] = {
    { ITEM_SKORPION,      { -0.6f,-0.5f,-1.0f }, { 1.1f, 1.0f, 12.0f } },
    { ITEM_AK47,         { -0.6f,-1.8f,-2.0f }, { 1.1f, 0.5f, 30.0f } },
    { ITEM_UZI,          { -0.6f,-3.5f,-1.0f }, { 1.1f, 0.0f, 12.0f } },
    { ITEM_MP5K,         { -0.6f,-1.5f,-2.0f }, { 1.1f,-1.0f, 23.0f } },
    { ITEM_MP5KSIL,      { -0.6f,-1.5f,-2.0f }, { 1.1f,-1.0f, 23.0f } },
    { ITEM_SPECTRE,      { -0.6f,-2.0f,-6.0f }, { 1.1f,-1.0f, 24.0f } },
    { ITEM_M16,          { -0.6f, 2.0f,-3.0f }, { 1.1f, 5.0f, 36.0f } },
    { ITEM_FNP90,        { -4.5f, 5.0f,-4.0f }, { 4.5f, 5.0f, 11.0f } },
    { ITEM_SHOTGUN,      { -0.6f,-2.0f,-3.0f }, { 1.1f, 0.0f, 33.0f } },
    { ITEM_AUTOSHOT,     { -0.6f,-1.0f,-2.0f }, { 1.1f, 1.0f, 30.0f } },
    { ITEM_SNIPERRIFLE,  { -1.2f, 5.0f,-2.0f }, { 1.7f, 6.0f, 35.0f } },
    { ITEM_LASER,        { -0.6f,-1.0f,-2.0f }, { 1.1f, 1.0f, 15.0f } },
    { ITEM_GRENADELAUNCH,{ -0.6f,-1.0f,-2.0f }, { 1.1f, 0.5f, 25.0f } },
};

s32 portAdsHandGrips(s32 item, coord3d *trigger, coord3d *support)
{
    s32 i;
    for (i = 0; i < sizeof(sAdsGrips) / sizeof(sAdsGrips[0]); i++) {
        if (sAdsGrips[i].item == item) {
            *trigger = sAdsGrips[i].trigger;
            *support = sAdsGrips[i].support;
            return TRUE;
        }
    }
    return FALSE;
}

static coord3d adsHandPoint(const Mtxf *gun, const coord3d *p)
{
    coord3d out;
    s32 i;
    for (i = 0; i < 3; i++) {
        out.f[i] = p->x * gun->m[0][i] + p->y * gun->m[1][i]
            + p->z * gun->m[2][i] + gun->m[3][i];
    }
    return out;
}

/* An eight-sided tapered section, shaded to match the original low-poly hands. */
static Gfx *adsHandSection(Gfx *gdl, coord3d a, coord3d b,
    f32 widthA, f32 heightA, f32 widthB, f32 heightB, const sImageTableEntry *material, f32 light, u8 alpha)
{
    static const f32 ring[8][2] = {
        {1.0f, 0.4f}, {0.4f, 1.0f}, {-0.4f, 1.0f}, {-1.0f, 0.4f},
        {-1.0f, -0.4f}, {-0.4f, -1.0f}, {0.4f, -1.0f}, {1.0f, -0.4f}
    };
    static const f32 shade[8] = {0.81f, 1.0f, 0.96f, 0.74f, 0.57f, 0.52f, 0.57f, 0.70f};
    extern float sqrtf(float);
    coord3d axis = { b.x-a.x, b.y-a.y, b.z-a.z };
    coord3d u, v;
    f32 length = sqrtf(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
    f32 span;
    Vtx *vertices;
    s32 end, i;

    if (length < 0.001f) return gdl;
    vertices = dynAllocateVertices(16);
    axis.x /= length; axis.y /= length; axis.z /= length;
    u.x = -axis.z; u.y = 0.0f; u.z = axis.x;
    span = sqrtf(u.x*u.x + u.z*u.z);
    if (span < 0.001f) { u.x = 1.0f; u.z = 0.0f; }
    else { u.x /= span; u.z /= span; }
    v.x = axis.y*u.z;
    v.y = axis.z*u.x - axis.x*u.z;
    v.z = -axis.y*u.x;

    for (end = 0; end < 2; end++) {
        const coord3d *c = end ? &b : &a;
        /* Fuller hands and forearms without moving either weapon grip. */
        f32 width = (end ? widthB : widthA) * 1.35f;
        f32 height = (end ? heightB : heightA) * 1.35f;
        for (i = 0; i < 8; i++) {
            Vtx *vertex = &vertices[end*8+i];
            f32 tone = shade[i] * light;
            s32 k;
            for (k = 0; k < 3; k++) {
                vertex->v.ob[k] = (s16)((c->f[k] + u.f[k]*ring[i][0]*width
                    + v.f[k]*ring[i][1]*height)*16.0f);
            }
            vertex->v.flag = 0;
            vertex->v.tc[0] = (s16)(i*(material->width-1)*32/7);
            vertex->v.tc[1] = end ? (material->height-1)*32 : 0;
            /* The original wardrobe texture supplies color, including skin. */
            vertex->v.cn[0] = vertex->v.cn[1] = vertex->v.cn[2] = (u8)(255*tone);
            vertex->v.cn[3] = alpha;
        }
    }
    gSPVertex(gdl++, osVirtualToPhysical(vertices), 16, 0);
    for (i = 0; i < 8; i += 2) {
        s32 j = (i+1)%8, k = (i+2)%8;
        gSP4Triangles(gdl++, i,j,j+8, i,j+8,i+8, j,k,k+8, j,k+8,j+8);
    }
    gSP4Triangles(gdl++, 0,2,1, 0,3,2, 0,4,3, 0,5,4);
    gSP4Triangles(gdl++, 0,6,5, 0,7,6, 8,9,10, 8,10,11);
    gSP4Triangles(gdl++, 8,11,12, 8,12,13, 8,13,14, 8,14,15);
    return gdl;
}

static void adsHandDrawGrip(Gfx **meshes, const Mtxf *gun, coord3d grip,
    s32 left, f32 blend, f32 light, const AdsWardrobe *style)
{
    f32 side = left ? 1.0f : -1.0f;
    const f32 armBulk = 1.30f;
    u8 alpha = (u8)(blend*255.0f);
    coord3d palm = grip;
    coord3d wrist, cuff, cuffEnd, elbow, knuckle, tip;
    s32 finger;

    palm.x += side*1.25f;
    wrist = palm;
    wrist.x += side*1.1f; wrist.y -= left ? 2.8f : 3.4f; wrist.z -= 2.0f;
    palm = adsHandPoint(gun, &palm);
    wrist = adsHandPoint(gun, &wrist);
    /* The elbow stays below the viewport; the grip follows the weapon, including recoil. */
    elbow.x = -side*(left ? 20.0f : 17.0f);
    elbow.y = -28.0f - (1.0f-blend)*18.0f;
    elbow.z = -12.0f;
    cuff = adsHandLerp(wrist, elbow, style->sleeveEnd);
    cuffEnd = adsHandLerp(wrist, elbow, style->cuffEnd);
    {
        const f32 w = (1.25f+1.85f*style->sleeveEnd)*armBulk;
        const f32 h = (1.10f+1.60f*style->sleeveEnd)*armBulk;
        const f32 ew = (1.25f+1.85f*style->cuffEnd)*armBulk;
        const f32 eh = (1.10f+1.60f*style->cuffEnd)*armBulk;
        const f32 hem = style->rolled ? 1.12f : 1.0f;
        meshes[ADS_SLEEVE] = adsHandSection(meshes[ADS_SLEEVE], elbow,cuff,
            3.1f*armBulk,2.7f*armBulk,w,h,&style->material[ADS_SLEEVE],light,alpha);
        meshes[ADS_CUFF] = adsHandSection(meshes[ADS_CUFF], cuff,cuffEnd,
            w*hem,h*hem,ew*hem,eh*hem,&style->material[ADS_CUFF],light,alpha);
        meshes[ADS_SKIN] = adsHandSection(meshes[ADS_SKIN], cuffEnd,wrist,
            ew,eh,1.25f*armBulk,1.1f*armBulk,&style->material[ADS_SKIN],light,alpha);
    }
    meshes[ADS_SKIN] = adsHandSection(meshes[ADS_SKIN], wrist,palm, 1.25f*armBulk,1.1f*armBulk,
        1.6f,1.35f, &style->material[ADS_SKIN],light,alpha);

    for (finger = 0; finger < 4; finger++) {
        /* Four curled fingers and a separate thumb give the grip a readable silhouette. */
        knuckle = grip; tip = grip;
        if (left) {
            knuckle.x += 1.8f; knuckle.y -= 0.1f; knuckle.z += (finger-1.5f)*0.8f;
            tip.x -= 1.3f; tip.y += 0.1f; tip.z = knuckle.z;
        } else {
            knuckle.x -= 1.8f; knuckle.y += 1.4f-finger*0.8f; knuckle.z += 0.8f;
            tip.x += 1.3f; tip.y = knuckle.y; tip.z += 1.4f;
        }
        knuckle = adsHandPoint(gun, &knuckle); tip = adsHandPoint(gun, &tip);
        meshes[ADS_SKIN] = adsHandSection(meshes[ADS_SKIN], knuckle,tip,
            0.42f,0.48f,0.35f,0.4f,&style->material[ADS_SKIN],light,alpha);
    }
    knuckle = grip; tip = grip;
    knuckle.x += side*1.6f; knuckle.y += 0.8f; knuckle.z -= 1.2f;
    tip.x += side*1.0f; tip.y += 2.2f; tip.z += 0.8f;
    knuckle = adsHandPoint(gun, &knuckle); tip = adsHandPoint(gun, &tip);
    meshes[ADS_SKIN] = adsHandSection(meshes[ADS_SKIN], knuckle,tip,
        0.65f,0.6f,0.45f,0.45f,&style->material[ADS_SKIN],light,alpha);
}

static void adsHandProjection(const Mtxf *scene, Mtxf *projection)
{
    s32 i;
    *projection = *scene;
    /* Reserve a near depth range for hands and gun, preserving the scene's XY projection. */
    for (i = 0; i < 4; i++) {
        projection->m[i][2] = scene->m[i][2]*0.01f - scene->m[i][3]*0.99f;
    }
}

Gfx *portAdsBeginHandLayer(Gfx *gdl)
{
    Mtxf projection;
    Mtx *matrix = dynAllocateMatrix();
    adsHandProjection(currentPlayerGetProjectionMatrixF(), &projection);
    matrix_4x4_f32_to_s32(projection.m, (s32 (*)[4])matrix);
    gSPMatrix(gdl++, osVirtualToPhysical(matrix), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    return gdl;
}

Gfx *portAdsDrawHands(Gfx *gdl, s32 item, const Mtxf *gun)
{
    coord3d trigger, support;
    Mtxf identity;
    Mtx *matrix;
    f32 blend = portAdsBlend();
    f32 light;
    AdsWardrobe style;
    Gfx *meshes[ADS_MATERIALS], *ends[ADS_MATERIALS];
    s32 i;
    if (blend <= 0.0f || !portAdsHandGrips(item, &trigger, &support)) return gdl;

    style = adsHandWardrobe(g_CurrentPlayer->bondtype, fileGetBondForCurrentFolder());

    matrix = dynAllocateMatrix();
    matrix_4x4_set_identity(&identity);
    matrix_scalar_multiply(1.0f/16.0f, identity.m[0]);
    matrix_4x4_f32_to_s32(identity.m, (s32 (*)[4])matrix);
    light = 0.55f + (g_CurrentPlayer->tileColor.r + g_CurrentPlayer->tileColor.g
        + g_CurrentPlayer->tileColor.b) * (0.45f/(255.0f*3.0f));
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_FOG);
    gSPSetGeometryMode(gdl++, G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);
    gSPTexture(gdl++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gSPMatrix(gdl++, osVirtualToPhysical(matrix), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    /* Per-material lists contain geometry only. Texture binding belongs to
     * the color pass, so it cannot override the transition's depth-only pass.
     * Each section emits eight commands; skin has seven sections per grip. */
    for (i = 0; i < ADS_MATERIALS; i++) {
        const s32 sections = i == ADS_SKIN ? 7 : 1;
        meshes[i] = ends[i] = dynAllocate((2*sections*8+1)*sizeof(Gfx));
    }
    adsHandDrawGrip(ends, gun,support, TRUE,blend,light, &style);
    adsHandDrawGrip(ends, gun,trigger, FALSE,blend,light, &style);
    for (i = 0; i < ADS_MATERIALS; i++) gSPEndDisplayList(ends[i]);

    if (blend < 1.0f) {
        /* The gun has already drawn. First write only the nearest hand depth,
         * leaving its colour underneath the fading hands intact. INTER allows
         * the colour pass to accept those exact same depths (LEQUAL on PC). */
        gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_INTER | Z_UPD, G_RM_AA_ZB_XLU_INTER2 | Z_UPD);
        gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gdl++, 0, 0, 0, 0, 0, 0);
        for (i = 0; i < ADS_MATERIALS; i++) gSPDisplayList(gdl++, osVirtualToPhysical(meshes[i]));
        gDPPipeSync(gdl++);
    }
    for (i = 0; i < ADS_MATERIALS; i++) {
        sImageTableEntry material = style.material[i];
        texSelect(&gdl, &material, 4, 0, 0);
        /* texSelect supplies the native format/palette and loads ROM pixels;
         * restore this layer's depth/blending after its generic setup. */
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);
        /* Bind the complete authored mip chain so CI palettes are read at
         * their correct offset, then sample tile zero for these close arms. */
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gSPTexture(gdl++, 0xffff, 0xffff, 0, G_TX_RENDERTILE, G_ON);
        gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
        if (blend == 1.0f) {
            gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
        } else {
            gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_INTER, G_RM_AA_ZB_XLU_INTER2);
        }
        gSPDisplayList(gdl++, osVirtualToPhysical(meshes[i]));
    }
    gDPPipeSync(gdl++);
    return gdl;
}
