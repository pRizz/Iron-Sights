/* Startup shader batch for KHR_parallel_shader_compile. Included from
 * gfx_opengl.cpp so it reuses that file's build steps unchanged: every program
 * is issued without a status query, and finished only once the browser reports
 * its link complete, because any earlier query blocks the page's main thread. */
#include <emscripten/html5.h>
#include "../web/shader_parallel.h"

#ifndef GL_COMPLETION_STATUS_KHR
#define GL_COMPLETION_STATUS_KHR 0x91B1
#endif

static std::vector<struct GlShaderBuild> web_shader_batch;

bool portWebShaderBatchSupported(void) {
    return emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(), "KHR_parallel_shader_compile");
}

size_t portWebShaderBatchIssue(uint64_t shader_id0, uint32_t shader_id1) {
    web_shader_batch.emplace_back();
    gfx_opengl_shader_build_begin(&web_shader_batch.back(), shader_id0, shader_id1);
    return web_shader_batch.size() - 1;
}

/* never blocks; a lost context reports every program complete */
bool portWebShaderBatchReady(size_t index) {
    GLint done = GL_FALSE;
    glGetProgramiv(web_shader_batch[index].program, GL_COMPLETION_STATUS_KHR, &done);
    return done != GL_FALSE;
}

struct ShaderProgram* portWebShaderBatchFinish(size_t index) {
    return gfx_opengl_shader_build_finish(&web_shader_batch[index]);
}

void portWebShaderBatchClear(void) {
    std::vector<struct GlShaderBuild>().swap(web_shader_batch);
}
