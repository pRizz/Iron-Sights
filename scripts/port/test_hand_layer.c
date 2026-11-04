/* Included by test_ads.c: the end of the first-person pistol depth layer. */

/* Ending the layer reloads only the scene projection. It must not clear
 * G_ZBUFFER: lvlRender sets it once per frame, so a clear here removed depth
 * testing from every later split-screen view (issue 5). */
static void testHandLayerEnd(void)
{
    Gfx list[4];
    Mtx sceneProjection;
    Gfx *end = portAdsEndHandLayer(list, &sceneProjection);
    assert(end == list + 1);
    assert((list[0].words.w0 >> 24) == G_MTX);
    assert(list[0].words.w1 == osVirtualToPhysical(&sceneProjection));
    assert(((list[0].words.w0 >> 16) & 0xff) == (G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH));
}
