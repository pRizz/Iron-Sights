#ifndef MIDAS_HD_TEXTURE_PREFETCH_H
#define MIDAS_HD_TEXTURE_PREFETCH_H
/* HD texture prefetch (FORK-053, scripts/port/late_loads.md): while a stage
 * loads, the download's images for that stage are read on worker threads, so
 * a texture first seen in play (Runway's shutter opening) only needs its GL
 * upload. The stage's images are its mission folder's in the scope (Surface
 * II and Bunker II share Surface's and Bunker's) plus the in-game effects,
 * plus the other folders' textures the stage was seen to draw
 * (texture_packs/hd_stage_prefetch.json, written by late_load_audit.py
 * --learn-prefetch); the title gets the gunbarrel. Custom packs keep their
 * own background reads. */
#include <cstdint>
#include <map>
#include <vector>

#include "hd_texture_pack.h"

namespace hd_textures {
namespace prefetch {
/* The scope groups a stage (a LEVELID) draws from, the effects included. */
std::vector<uint8_t> groups_for_stage(int stage);
/* Render thread, after every (re)index, with nothing when the pack is off:
 * the download's entries by native key, and the index's generation. */
void index_ready(std::map<Key, Entry> downloads, uint32_t generation);
/* Game thread, as a stage starts loading: read its images, drop others. */
void request_stage(int stage);
/* Game thread, at the end of the stage load: wait until every requested
 * image is read; false if timeout_ms passed first. */
bool wait(unsigned timeout_ms);
/* The stage last requested (GE_HD_USED's records), and whether an entry is
 * in that stage's own groups rather than another folder's. */
int stage();
bool own(int stage, const Entry &entry);
enum class State { none, ready, pending };
/* Render thread: the entry's image, moved into image when ready (once). */
State find(const Entry &entry, Image &image);
}  // namespace prefetch
}  // namespace hd_textures
#endif
