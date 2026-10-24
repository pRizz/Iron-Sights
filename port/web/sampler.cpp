/* WebGL sampler capabilities and per-draw transparent-border uniforms. */
#include "../fast3d/glad/glad.h"
#include <emscripten/html5.h>
#include <unordered_set>
#include "sampler.h"

static GLfloat borderFlags[2][3];
static std::unordered_set<GLuint> uploadedTextures;

void portWebSamplerFind(uint32_t program, int locations[2])
{
    locations[0] = glGetUniformLocation(program, "uWebBorder0");
    locations[1] = glGetUniformLocation(program, "uWebBorder1");
}

void portWebSamplerSync(const int locations[2])
{
    for (int tile = 0; tile < 2; tile++) {
        if (locations[tile] >= 0) glUniform3fv(locations[tile], 1, borderFlags[tile]);
    }
}

void portWebSamplerFlags(int tile, uint32_t cms, uint32_t cmt, bool linear)
{
    if (tile < 0 || tile >= 2) return;
    borderFlags[tile][0] = cms == 0x10;
    borderFlags[tile][1] = cmt == 0x10;
    borderFlags[tile][2] = linear;
}

uint32_t portWebMirrorClamp(void)
{
    const auto context = emscripten_webgl_get_current_context();
    emscripten_webgl_enable_extension(context, "EXT_texture_filter_anisotropic");
    return emscripten_webgl_enable_extension(context, "EXT_texture_mirror_clamp_to_edge")
        ? GL_MIRROR_CLAMP_TO_EDGE : GL_MIRRORED_REPEAT;
}

void portWebTextureUploaded(uint32_t id) { if (id) uploadedTextures.insert(id); }
void portWebTextureDeleted(uint32_t id) { uploadedTextures.erase(id); }
bool portWebTextureHasPixels(uint32_t id) { return uploadedTextures.count(id) != 0; }
