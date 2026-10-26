/* Included by test_ads.c so the normal native/CI ADS gate covers both knife styles. */
#include "../../src/game/matrixmath.c"
extern f32 fabsf(f32 value);

static void assertKnifeNear(f32 a, f32 b) { assert(fabsf(a - b) < 0.0001f); }

static u8 knifeDrawArena[32768] __attribute__((aligned(16)));
static s32 knifeDrawOffset, knifeDrawPatched, knifeDrawCalls;
static ModelNode *knifeDrawNodes;
static union ModelRoData *knifeDrawData;
void *dynAllocate(s32 size)
{
    void *result = knifeDrawArena + knifeDrawOffset;
    knifeDrawOffset += (size + 15) & ~15;
    assert(knifeDrawOffset <= sizeof(knifeDrawArena));
    return result;
}
void subdraw(ModelRenderData *renderdata, Model *model)
{
    s32 i;
    ++knifeDrawCalls;
    for (i = 0; i < 3; i++) {
        const ModelRoData_DisplayListRecord *data = &knifeDrawNodes[i].Data->DisplayList;
        assert(data->Vertices == knifeDrawData[i].DisplayList.Vertices);
        assert(data->ModelType == knifeDrawData[i].DisplayList.ModelType);
        assert((data->Primary != knifeDrawData[i].DisplayList.Primary) == knifeDrawPatched);
        assert((data->Primary[0].words.w1 != 0x03000000) == knifeDrawPatched);
        assert(!memcmp(&data->Primary[1], &knifeDrawData[i].DisplayList.Primary[1], 2*sizeof(Gfx)));
    }
}

static void testKnifeDrawCopies(void)
{
    Gfx commands[3] = {{{0}}};
    ModelNode nodes[3] = {0}, switches[2] = {0}, *table[16] = {0};
    union ModelRoData data[3] = {0};
    ModelFileHeader header = {0};
    Model model = {0};
    ModelRenderData renderdata = {0};
    RenderPosView matrix = {0};
    s32 i;
    commands[0].words.w0 = G_MTX << 24;
    commands[0].words.w1 = 0x03000000;
    commands[1].words.w0 = (u8)G_TRI1 << 24;
    commands[1].words.w1 = 0x00000a14;
    commands[2].words.w0 = (u8)G_ENDDL << 24;
    for (i = 0; i < 3; i++) {
        data[i].DisplayList.Primary = commands;
        data[i].DisplayList.ModelType = 3;
        nodes[i].Opcode = MODELNODE_OPCODE_DL;
        nodes[i].Data = &data[i];
    }
    switches[0].Child = &nodes[0];
    nodes[0].Next = &nodes[1];
    switches[1].Child = &nodes[2];
    table[14] = &switches[0]; table[15] = &switches[1];
    header.Switches = table; header.numSwitches = 16;
    model.obj = &header; model.render_pos = &matrix;
    matrix_4x4_set_identity(&matrix.pos);
    knifeDrawNodes = nodes; knifeDrawData = data;
    knifeDrawCalls = knifeDrawOffset = 0;
    knifeStates[0].turn = 1;
    knifeDrawPatched = TRUE;
    portKnifeAdsDraw(&renderdata, &model, ITEM_THROWKNIFE, GUNRIGHT);
    assert(knifeDrawCalls == 1);
    for (i = 0; i < 3; i++) assert(nodes[i].Data == &data[i]);
    assert(commands[0].words.w1 == 0x03000000); /* never mutate ROM material/triangle/matrix lists */
    knifeDrawPatched = FALSE;
    portKnifeAdsDraw(&renderdata, &model, ITEM_WPPK, GUNRIGHT);
    portKnifeAdsDraw(&renderdata, &model, ITEM_THROWKNIFE, GUNLEFT);
    assert(knifeDrawCalls == 3);
    /* An unfamiliar nested list must roll back already patched siblings. */
    {
        Gfx unknown[3];
        memcpy(unknown, commands, sizeof(commands));
        unknown[1].words.w0 = G_DL << 24;
        data[2].DisplayList.Primary = unknown;
        portKnifeAdsDraw(&renderdata, &model, ITEM_THROWKNIFE, GUNRIGHT);
        for (i = 0; i < 3; i++) assert(nodes[i].Data == &data[i]);
    }
}

static void testKnifeAds(void)
{
    const s32 rates[] = {30, 60, 120, 240};
    s32 style, rate, axis, setting;
    const s32 option = portOptFind("knife_ads");
    assert(option >= 0 && portOptAvailable(option));
    assert(portOptCategoryOf(option) == PORTOPT_CAT_GAMEPLAY);
    assert(portKnifeAdsOptionGet("knife_ads", &setting) && setting == 0);
    assert(strcmp(portOptValueText(option), "Butt Centered\n") == 0);
    portOptStep(option, 1);
    assert(portOptGet(option) == 1 && savedKnifeStyle == 1);
    assert(strcmp(portOptValueText(option), "Tip Centered\n") == 0);
    assert(portJoyPortSetOption("knife_ads", "0") && portOptGet(option) == 0);

    for (style = 0; style <= 1; style++) for (rate = 0; rate < 4; rate++) {
        const f32 delta = 60.0f / rates[rate];
        const coord3d source = style ? (coord3d){1, -97, 1} : (coord3d){1, 338, 150};
        coord3d turned, camera, origin;
        Mtxf local, basis;
        s32 i;
        portKnifeAdsReset();
        selectPlayer(0);
        g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_THROWKNIFE;
        g_CurrentPlayer->hands[GUNRIGHT].weapon_ammo_in_magazine = 1;
        portJoyPortOptionSet("knife_ads", style);
        for (i = 0; i < rates[rate]; i++) portKnifeAdsTick(0, TRUE, 1, delta);
        assertKnifeNear(knifeStates[0].aim, 1);
        assertKnifeNear(knifeStates[0].turn, style);
        /* The actual pose cancels the sight's projected X/Y under a rotated basis. */
        matrix_4x4_set_rotation_around_x(-0.08f, &basis);
        origin = (coord3d){14, -24.8f, -34};
        portKnifeAdsPose(GUNRIGHT, ITEM_THROWKNIFE, &basis, &origin);
        knifeLocalTransform(knifeStates[0].turn, &local);
        knifePoint(&local, &source, &turned);
        knifePoint(&basis, &turned, &camera);
        assertKnifeNear(origin.x + camera.x * .1f, 0);
        assertKnifeNear(origin.y + camera.y * .1f, 0);
        if (!style) assertKnifeNear(origin.z, -34); /* simple mode only translates X/Y */
        /* Reversing aim mid-raise must walk back from the current pose. */
        portKnifeAdsReset();
        portKnifeAdsTick(0, TRUE, .4f, delta);
        {
            const f32 previous = knifeStates[0].aim;
            portKnifeAdsTick(0, TRUE, 0, delta);
            assert(knifeStates[0].aim <= previous && knifeStates[0].aim >= 0);
            assert(previous - knifeStates[0].aim <= delta / 6 + .0001f);
        }
        for (i = 0; i < rates[rate]; i++) portKnifeAdsTick(0, TRUE, 1, delta);
        /* Committing then releasing aim during a throw finishes the regrip. */
        g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_DRAW;
        for (i = 0; i < rates[rate]; i++) {
            portKnifeAdsTick(0, TRUE, i ? 0 : 1, delta);
            assert(knifeStates[0].turn >= 0 && knifeStates[0].turn <= 1);
        }
        assertKnifeNear(knifeStates[0].turn, 0);
        assertKnifeNear(knifeStates[0].aim, 0);
        assert(g_CurrentPlayer->hands[GUNRIGHT].weapon_ammo_in_magazine == 1);
        g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_RECOVER;
        portKnifeAdsTick(0, TRUE, 1, delta);
        assertKnifeNear(knifeStates[0].turn, 0);
        assertKnifeNear(knifeStates[0].aim, 0);
        g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_IDLE;
        portKnifeAdsTick(0, TRUE, 1, delta);
        assert(knifeStates[0].aim > 0); /* re-aim only when the native recovery completes */
        portKnifeAdsTick(0, FALSE, 1, delta);
        assertKnifeNear(knifeStates[0].aim, 0); /* watch/death/switch cancels presentation */
    }
    {
        const coord3d handle = {0,270,150}, blade = {0,50,65};
        const coord3d a = {-5,-97,1}, b = {6,184,129};
        coord3d grip, pa, pb;
        Mtxf local;
        f32 before = 0, after = 0;
        knifeLocalTransform(1, &local);
        knifePoint(&local, &handle, &grip);
        knifePoint(&local, &a, &pa);
        knifePoint(&local, &b, &pb);
        for (axis = 0; axis < 3; axis++) {
            assertKnifeNear(grip.f[axis], blade.f[axis]);
            before += (a.f[axis]-b.f[axis])*(a.f[axis]-b.f[axis]);
            after += (pa.f[axis]-pb.f[axis])*(pa.f[axis]-pb.f[axis]);
        }
        assert(fabsf(before-after) < .05f); /* a rigid rotation never stretches the blade */
    }
    portKnifeAdsReset();
    selectPlayer(0);
    testKnifeDrawCopies();
    portKnifeAdsReset();
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_THROWKNIFE;
    portKnifeAdsTick(0, TRUE, 1, 9);
    selectPlayer(1);
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_THROWKNIFE;
    portKnifeAdsTick(1, TRUE, 0, 9);
    assert(knifeStates[0].aim == 1 && knifeStates[1].aim == 0);
    portStateResetInput();
    assert(knifeStates[0].aim == 0 && knifeStates[1].aim == 0);
    portJoyPortOptionSet("knife_ads", 0);
    selectPlayer(0);
    puts("Knife ADS checks passed: defaults/settings, sight projection, grip/rigidity, interrupted throws, recovery, per-player reset and four tick rates.");
}
