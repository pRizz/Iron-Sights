/* Browser platform contracts and lifecycle. No native rendering behavior. */
#include <stdlib.h>
#include <emscripten.h>
#include <emscripten/heap.h>
#include <emscripten/stack.h>
#include <bondconstants.h>
#include "boss.h"
#include "port_hostptr.h"

static s32 webPaused;

void portWebCheckDma(u32 device, void *memory, u32 size)
{
    const u32 address = (u32)memory;
    const u32 capacity = (u32)emscripten_get_heap_size();
    const u32 stackBase = (u32)emscripten_stack_get_base();
    const u32 stackEnd = (u32)emscripten_stack_get_end();
    if (device < capacity && size <= capacity-device && address < capacity
            && size <= capacity-address
            && !(address >= stackEnd && address < stackBase && size > stackBase-address)) return;
    fprintf(stderr, "ge: web invalid DMA: device=%08x memory=%08x bytes=%u stack=%08x..%08x capacity=%u\n",
            device, address, size, stackEnd, stackBase, capacity);
    __builtin_trap();
}

static void check_span(const char *name, const u8 *start, const u8 *end, u32 expected)
{
    if ((u32)(end - start) == expected && PORT_IS_HOST_ADDR(start)) return;
    fprintf(stderr, "ge: web layout failed: %s span=%u expected=%u\n",
            name, (u32)(end-start), expected);
    exit(2);
}

void portWebCheckLayout(void)
{
    extern u8 _GlobalimagetableSegmentStart[], _GlobalimagetableSegmentEnd[];
    extern u8 _fontbankgothicSegmentStart[], _fontbankgothicSegmentEnd[];
    extern u8 _fontzurichboldSegmentStart[], _fontzurichboldSegmentEnd[];
    extern u8 _fontdlSegmentRomStart[], _fontdlSegmentRomEnd[];
    extern u8 _rarewarelogoSegmentStart[], _rarewarelogoSegmentEnd[];
    check_span("boot", (u8 *)&_codeSegmentRomStart, (u8 *)&_inflateSegmentRomStart, 16);
    check_span("image table", _GlobalimagetableSegmentStart, _GlobalimagetableSegmentEnd, 0x1400);
    check_span("animation", (u8 *)&_animation_dataSegmentStart, (u8 *)&_animation_dataSegmentEnd, 0xe7e0);
    check_span("gothic", _fontbankgothicSegmentStart, _fontbankgothicSegmentEnd, 0x24b0);
    check_span("zurich", _fontzurichboldSegmentStart, _fontzurichboldSegmentEnd, 0x3540);
    check_span("font display lists", _fontdlSegmentRomStart, _fontdlSegmentRomEnd, 0xc0);
    check_span("logo", _rarewarelogoSegmentStart, _rarewarelogoSegmentEnd, 0x67f0);
    printf("ge: web: layout contracts passed; linear memory %zu bytes\n", emscripten_get_heap_size());
}

EMSCRIPTEN_KEEPALIVE void portWebSetPaused(s32 paused) { webPaused = paused != 0; }
EMSCRIPTEN_KEEPALIVE s32 portWebGetStage(void) { return bossGetStageNum(); }
EMSCRIPTEN_KEEPALIVE u32 portWebGetMemoryBytes(void) { return (u32)emscripten_get_heap_size(); }
EMSCRIPTEN_KEEPALIVE u32 portWebGetFrames(void)
{
    extern u32 g_PortFramesPumped;
    return g_PortFramesPumped;
}
EMSCRIPTEN_KEEPALIVE u32 portWebGetGfxTasks(void)
{
    extern u32 g_PortGfxTasksSubmitted;
    return g_PortGfxTasksSubmitted;
}

void portWebWaitIfPaused(void)
{
    while (webPaused) emscripten_sleep(50);
}

void portWebAuditFrameLimit(void)
{
    printf("{\"web_frame_limit\":true,\"stage\":%d,\"mission_is_dam\":%s,\"linear_bytes\":%zu}\n",
            bossGetStageNum(), bossGetStageNum() == LEVELID_DAM ? "true" : "false",
            emscripten_get_heap_size());
}
