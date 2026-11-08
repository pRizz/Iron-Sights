/* Player-installed HD texture packs (FORK-053, restored from FORK-033): the
 * renderer adapter. Included after the native tile/cache helpers. */
#include "port_hd_textures.h"
/* A custom pack's image being read for a cache node (hd_request): what the
 * upload needs from the tile it was asked for. */
struct GeHdPending {
    unsigned width, height, cropWidth, cropHeight;
};
static std::unordered_map<uint32_t, GeHdPending> ge_hd_pending;

/* Upload an HD image for the selected texture, cropped like ge_upload_masked
 * at the authored scale, and record its size and scale on the node. */
static void ge_hd_upload(TextureCacheValue *value, hd_textures::Image &replacement, const GeHdPending &tile)
{
    const unsigned uploadWidth = tile.cropWidth*replacement.width/tile.width;
    const unsigned uploadHeight = tile.cropHeight*replacement.height/tile.height;
    for (unsigned y=0; y<uploadHeight; ++y)
        std::memmove(replacement.rgba.data()+size_t(y)*uploadWidth*4,
            replacement.rgba.data()+size_t(y)*replacement.width*4, size_t(uploadWidth)*4);
    gfx_rapi->upload_texture(replacement.rgba.data(), uploadWidth, uploadHeight);
    if (value) {
        value->hd_bytes = uint32_t(size_t(uploadWidth)*uploadHeight*16/3+16);
        value->hd_scale_s = float(replacement.width) / float(tile.width);
        value->hd_scale_t = float(replacement.height) / float(tile.height);
        value->hd_request = 0;
    }
}

static bool ge_hd_try_import(int i, int tile, bool firstPerson)
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
        const bool maskWrapped = ge_maskwrap_tile(tile, width);
        const bool waterWrap = fmt == G_IM_FMT_RGBA && siz == G_IM_SIZ_16b && rdp.texture_tile[tile].masks &&
            rdp.texture_tile[tile].maskt && (2u << rdp.texture_tile[tile].masks) != line;
        if (firstPerson) { /* the weapon and hands keep the game's textures (gunfire.c tag) */
            hd_textures::kept(source);
            return false;
        }
        if (waterWrap) return false;
        /* Mirror ge_upload_masked's logical crop at the authored scale; a
         * mask-wrapped tile draws at its mask size, which a masked image covers. */
        const auto& parameters = rdp.texture_tile[tile];
        const unsigned maskWidth = 1u << parameters.masks, maskHeight = 1u << parameters.maskt;
        GeHdPending crop = maskWrapped ? GeHdPending{maskWidth, maskHeight, maskWidth, maskHeight}
                                       : GeHdPending{width, height, width, height};
        if (!maskWrapped && ge_maskcrop_enabled()) {
            if (parameters.masks) crop.cropWidth = std::min(crop.cropWidth, 1u << parameters.masks);
            if (parameters.maskt) crop.cropHeight = std::min(crop.cropHeight, 1u << parameters.maskt);
        }
        hd_textures::Image replacement;
        uint32_t request = 0;
        switch (hd_textures::lookup(source, replacement, request, maskWrapped)) {
            case hd_textures::Found::now:
                ge_hd_upload(rendering_state.textures[i] ? &rendering_state.textures[i]->second : nullptr,
                             replacement, crop);
                return true;
            case hd_textures::Found::later:
                /* The original uploads now; ge_hd_start_frame swaps the image in. */
                if (rendering_state.textures[i]) {
                    rendering_state.textures[i]->second.hd_request = request;
                    ge_hd_pending[request] = crop;
                }
                return false;
            case hd_textures::Found::none:
                return false;
        }
    }
    return false;
}

/* Put finished custom-pack images into the nodes that asked for them, in
 * place of their originals; a node evicted meanwhile is skipped. */
static void ge_hd_swap_finished()
{
    hd_textures::Image image;
    uint32_t request = 0;
    bool swapped = false;
    while (hd_textures::finished(request, image)) {
        const auto pending = ge_hd_pending.find(request);
        if (pending == ge_hd_pending.end()) continue;
        const GeHdPending tile = pending->second;
        ge_hd_pending.erase(pending);
        for (auto& entry : gfx_texture_cache.map) {
            if (entry.second.hd_request != request) continue;
            gfx_rapi->select_texture(0, entry.second.texture_id, entry.second.linear_filter);
            ge_hd_upload(&entry.second, image, tile);
            swapped = true;
            break;
        }
    }
    if (swapped) {
        /* GL's unit 0 binding moved: re-select on the next draw, and have
         * retained lists revalidate (their clamps used the native scale). */
        rendering_state.textures[0] = NULL;
        rdp.textures_changed[0] = true;
        ++ge_state_serial;
        ++ge_texcache_gen;
    }
}

static void ge_hd_start_frame()
{
    if (hd_textures::beginFrame()) {
        gfx_texture_cache_clear();
        ge_hd_pending.clear();
        ++ge_state_serial;
    }
    if (!portTexturePackGet()) return;
    ge_hd_swap_finished();
    /* HD images are budgeted in bytes, independent of the inherited count cap.
     * Evict at frame boundaries; retained lists validate the cache generation. */
    {
        uint64_t bytes = 0;
        for (const auto& entry : gfx_texture_cache.map) bytes += entry.second.hd_bytes;
        ge_stat_hd_kb = (uint32_t) (bytes / 1024);  /* GE_MEMSTATS */
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
