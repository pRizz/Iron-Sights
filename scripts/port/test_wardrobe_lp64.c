/* Included by test_wardrobe.c (LP64-003 task 11). At 64 bits a texture or
 * matrix word in a display list is a whole host address or segment word. The
 * arm and grip code must carry it whole: a word that equals a skin texture or
 * a segment-3 matrix only in its low 32 bits is another address and must not
 * be taken for one. At 32 bits a word is the whole address, so there is
 * nothing more to check. */
#include "platform_info.h"
#if IS_64_BIT
#define LP64_HIGH_WORD ((uintptr_t) 1 << 32)

/* The skin textures' data sit at 1001 and 1002 (fixture). */
static void lp64SkinImageChecks(void)
{
    // Arrange
    const uintptr_t skin = 1001, aliased = 1001 + LP64_HIGH_WORD;

    // Act
    s32 armSkin = armSkinImage(skin, PORT_ARM_HAND), armAliased = armSkinImage(aliased, PORT_ARM_HAND);
    s32 gripSkin = portArmGripSkin(skin), gripAliased = portArmGripSkin(aliased);

    // Assert
    assert(armSkin && !armAliased);
    assert(gripSkin && !gripAliased);
}

/* paDecode keeps the hand's matrix word whole and accepts only segment 3. */
static void lp64MatrixWordChecks(void)
{
    // Arrange
    Gfx dl[2]; Vtx vertex[1] = {0};
    ModelRoData_DisplayListRecord data = {0};
    uintptr_t matrix = 0, aliasedMatrix = 0;
    s32 plain, aliased;
    data.Primary = dl; data.Vertices = (Vertex *) vertex; data.BaseAddr = vertex; data.numVertices = 1;
    gSPEndDisplayList(&dl[1]);

    // Act
    gSPMatrix(&dl[0], 0x03000040, G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    plain = paDecode(&data, &matrix);
    gSPMatrix(&dl[0], LP64_HIGH_WORD + 0x03000040, G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    aliased = paDecode(&data, &aliasedMatrix);

    // Assert
    assert(plain == 0 && matrix == 0x03000040);
    assert(aliased == -1);
}

/* A native skin triangle keeps its texture address whole (fit: a fitted pose
 * whose first skin triangle samples image 1001). */
static void lp64SurfaceImageCheck(const NativeArmPose *fit)
{
    // Arrange
    static NativeArmPose pose;
    Vtx mapped = {0}, saved;
    pose = *fit; pose.skinCount = 1; pose.skin[0].image = 1001 + LP64_HIGH_WORD;
    saved = mapped;

    // Act
    paNativeSurface(&pose.sourceMin, 1001, &mapped, 1, &pose);

    // Assert
    assert(!memcmp(&saved, &mapped, sizeof(mapped)));
}

/* A closed cuff keeps its clothing texture address whole (settimg: the
 * source list's texture command, set to image 1050 by the caller). */
static void lp64CuffImageCheck(const ModelRoData_DisplayListRecord *data, s32 length, Gfx *settimg)
{
    // Arrange
    ArmCuffCap cap = {0};
    gDPSetTextureImage(settimg, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 1050 + LP64_HIGH_WORD);

    // Act
    armFindCuff(data, length, &cap);
    gDPSetTextureImage(settimg, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 1050);

    // Assert
    assert(cap.count == 6 && cap.image == 1050 + LP64_HIGH_WORD);
}

static void lp64ArmWordChecks(void)
{
    lp64SkinImageChecks();
    lp64MatrixWordChecks();
}
#else
static void lp64SurfaceImageCheck(const NativeArmPose *fit) { (void) fit; }
static void lp64CuffImageCheck(const ModelRoData_DisplayListRecord *data, s32 length, Gfx *settimg)
{ (void) data; (void) length; (void) settimg; }
static void lp64ArmWordChecks(void) {}
#endif
