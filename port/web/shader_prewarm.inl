/* A single compile can exceed the per-frame budget. Finish the existing list
 * behind browser loading progress instead of stalling every visible logo frame.
 * Desktop retains its incremental prewarm and its current frame scheduling. */
extern "C" void portWebPrepareShaders(void) {
    if (ge_prewarm_on < 0) gfx_shader_prewarm_load();
    const unsigned total = (unsigned)ge_prewarm_ids.size();
    portWebStartupProgress("Preparing graphics", 0, total);
    while (ge_prewarm_on == 1 && ge_prewarm_next < ge_prewarm_ids.size()) {
        const auto id = ge_prewarm_ids[ge_prewarm_next++];
        gfx_lookup_or_create_shader_program(id.first, id.second);
        portWebStartupProgress("Preparing graphics", (unsigned)ge_prewarm_next, total);
    }
    printf("ge: web: startup shader preparation complete (%u programs)\n", (unsigned)ge_prewarm_next);
    portWebStartupProgress("Starting GoldenEye", 0, 0);
}
