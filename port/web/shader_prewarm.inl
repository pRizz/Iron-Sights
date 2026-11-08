#include "../web/shader_parallel.h"

/* KHR_parallel_shader_compile: issue listed programs without a status query,
 * then yield until the browser reports each link complete. Queries before that
 * point block the page's main thread (85-140 ms per cold program in Chrome),
 * so only completed programs are finished and loaded. A small window of
 * programs in flight keeps the browser's GPU process free for the room's own
 * frames, which otherwise queue behind the whole list. */
#ifndef MIDAS_WEB_SHADER_WINDOW
#define MIDAS_WEB_SHADER_WINDOW 8
#endif
static void portWebPrepareShadersParallel(unsigned total) {
    std::vector<size_t> waiting;
    unsigned long long issue_us = 0;
    unsigned long long slowest_us = 0;
    unsigned done = 0;
    for (;;) {
        const unsigned long long issue_t0 = portProfNow();
        while (waiting.size() < MIDAS_WEB_SHADER_WINDOW && ge_prewarm_next < ge_prewarm_ids.size()) {
            const auto id = ge_prewarm_ids[ge_prewarm_next++];
            if (gfx_rapi->lookup_shader(id.first, id.second) == NULL) {
                waiting.push_back(portWebShaderBatchIssue(id.first, id.second));
            } else {
                done++;
            }
        }
        issue_us += portProfNow() - issue_t0;
        if (waiting.empty()) {
            break;
        }
        portWebStartupProgress("Preparing graphics", done, total);
        /* finishing costs about 1 ms of lookups per program; a warm cache
         * completes many at once, so bound each slice */
        const unsigned long long slice_t0 = portProfNow();
        for (size_t i = 0; i < waiting.size() && portProfNow() - slice_t0 < 8000;) {
            if (!portWebShaderBatchReady(waiting[i])) {
                i++;
                continue;
            }
            /* gfx_lookup_or_create_shader_program's bookkeeping; the id is already listed */
            gfx_rapi->unload_shader(rendering_state.shader_program);
            rendering_state.shader_program = portWebShaderBatchFinish(waiting[i]);
            ge_state_serial++;
            waiting.erase(waiting.begin() + (std::ptrdiff_t)i);
            done++;
        }
        slowest_us = std::max(slowest_us, portProfNow() - slice_t0);
    }
    portWebStartupProgress("Preparing graphics", done, total);
    printf("ge: web: shader batch window %u; issuing %llu ms; longest finish slice %llu ms\n",
           (unsigned)MIDAS_WEB_SHADER_WINDOW, issue_us / 1000, slowest_us / 1000);
    portWebShaderBatchClear();
}

/* A single compile can exceed the per-frame budget. Finish the existing list
 * behind browser loading progress instead of stalling every visible logo frame.
 * Without the extension each program still completes before the next yield.
 * Desktop retains its incremental prewarm and its current frame scheduling. */
extern "C" int portIconStagePending(void); /* weapon_cache.cpp */
extern "C" void portWebPrepareShaders(void) {
    if (ge_prewarm_on < 0) gfx_shader_prewarm_load();
    const unsigned total = (unsigned)ge_prewarm_ids.size();
    const unsigned long long t0 = portProfNow();
    portWebStartupProgress("Preparing graphics", 0, total);
    const bool parallel = ge_prewarm_on == 1 && portWebShaderBatchSupported();
    if (parallel) portWebPrepareShadersParallel(total);
    while (ge_prewarm_on == 1 && ge_prewarm_next < ge_prewarm_ids.size()) {
        const auto id = ge_prewarm_ids[ge_prewarm_next++];
        gfx_lookup_or_create_shader_program(id.first, id.second);
        portWebStartupProgress("Preparing graphics", (unsigned)ge_prewarm_next, total);
    }
    printf("ge: web: startup shader preparation complete (%u programs, %s, %llu ms)\n",
           (unsigned)ge_prewarm_next, parallel ? "parallel" : "sequential", (portProfNow() - t0) / 1000);
    /* FORK-019: an icon stage armed for this boot is the next step shown */
    if (portIconStagePending()) portWebStartupProgress("Preparing weapon icons", 0, 38);
    else portWebStartupProgress("Starting GoldenEye", 0, 0);
}
