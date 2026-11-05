#ifndef MIDAS_WEB_SHADER_PARALLEL_H
#define MIDAS_WEB_SHADER_PARALLEL_H
/* Startup shader batch for KHR_parallel_shader_compile (shader_parallel.inl). */
#include <stddef.h>
#include <stdint.h>
struct ShaderProgram;
bool portWebShaderBatchSupported(void);
size_t portWebShaderBatchIssue(uint64_t shader_id0, uint32_t shader_id1);
bool portWebShaderBatchReady(size_t index);
struct ShaderProgram* portWebShaderBatchFinish(size_t index);
void portWebShaderBatchClear(void);
#endif
