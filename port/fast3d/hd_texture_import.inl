/* Native HD renderer adapter. Included after the native tile/cache helpers. */
#include "port_hd_textures.h"
static bool ge_hd_try_import(int i, int tile)
{
    const auto& loaded_texture = rdp.loaded_texture[rdp.texture_tile[tile].tmem];
    const auto fmt = rdp.texture_tile[tile].fmt;
    const auto siz = rdp.texture_tile[tile].siz;
    const auto palette_index = rdp.texture_tile[tile].palette;
    /* Preserve native UVs, wrap masks and shader; only the uploaded image grows. */
    if (!s_GeTextGlyphs && fmt != G_IM_FMT_YUV && (fmt != G_IM_FMT_CI || rdp.palette_fmt != G_TT_NONE)) {
        const unsigned line = rdp.texture_tile[tile].line_size_bytes;
        const unsigned stride = siz == G_IM_SIZ_32b ? line*2 : line;
        const unsigned width = (stride << 1) >> siz;
        const unsigned height = stride ? loaded_texture.size_bytes / stride : 0;
        const unsigned palette = siz == G_IM_SIZ_4b ? unsigned(palette_index & 15u)*16 : 0;
        hd_textures::Tile source{loaded_texture.addr, loaded_texture.size_bytes, width, height,
            stride, fmt, siz, rdp.palette+palette, siz == G_IM_SIZ_4b ? 16u : 256u};
        hd_textures::Image replacement;
        const bool specialWrap = ge_maskwrap_tile(tile, width) || (fmt == G_IM_FMT_RGBA &&
            siz == G_IM_SIZ_16b && rdp.texture_tile[tile].masks && rdp.texture_tile[tile].maskt &&
            (2u << rdp.texture_tile[tile].masks) != line);
        if (!specialWrap && hd_textures::lookup(source, replacement)) {
            /* Mirror ge_upload_masked's logical crop at the authored scale. */
            unsigned cropWidth = width, cropHeight = height;
            const auto& parameters = rdp.texture_tile[tile];
            if (ge_maskcrop_enabled()) {
                if (parameters.masks) cropWidth = std::min(cropWidth, 1u << parameters.masks);
                if (parameters.maskt) cropHeight = std::min(cropHeight, 1u << parameters.maskt);
            }
            const unsigned uploadWidth = cropWidth*replacement.width/width;
            const unsigned uploadHeight = cropHeight*replacement.height/height;
            for (unsigned y=0; y<uploadHeight; ++y)
                std::memmove(replacement.rgba.data()+size_t(y)*uploadWidth*4,
                    replacement.rgba.data()+size_t(y)*replacement.width*4, size_t(uploadWidth)*4);
            gfx_rapi->upload_texture(replacement.rgba.data(), uploadWidth, uploadHeight);
            if (rendering_state.textures[i]) rendering_state.textures[i]->second.hd_bytes =
                uint32_t(size_t(uploadWidth)*uploadHeight*16/3+16);
            return true;
        }
    }
    return false;
}

static void ge_hd_start_frame()
{
    if (hd_textures::beginFrame()) {
        gfx_texture_cache_clear();
        ++ge_state_serial;
    }
    if (!portTexturePackGet()) return;
    /* HD images are budgeted in bytes, independent of the inherited count cap.
     * Evict at frame boundaries; retained lists validate the cache generation. */
    {
        uint64_t bytes = 0;
        for (const auto& entry : gfx_texture_cache.map) bytes += entry.second.hd_bytes;
        for (auto it = gfx_texture_cache.lru.begin(); bytes > 128u*1024*1024 && it != gfx_texture_cache.lru.end();) {
            const auto victim = it++->it;
            if (!victim->second.hd_bytes) continue;
            bool bound = false;
            for (const auto *node : rendering_state.textures) if (node == &*victim) bound = true;
            if (bound) continue;
            bytes -= victim->second.hd_bytes;
            gfx_rapi->delete_texture(victim->second.texture_id);
            gfx_texture_cache.lru.erase(victim->second.lru_location);
            gfx_texture_cache.map.erase(victim);
            ++ge_texcache_gen;
        }
    }
}
