#if !defined(__EMSCRIPTEN__) && !defined(GE_XBOX)
#include "hd_textures.h"
#include "port_hd_textures.h"
#include <atomic>
#include <map>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <filesystem>
#ifdef __linux__
static_assert(sizeof(off_t) >= 8 && sizeof(time_t) >= 8, "HD loader requires Linux large-file/time metadata");
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif
namespace {
std::atomic<int> requested{1};
std::string directory;
std::string bundledDirectory, currentPath;
int current = -1;
unsigned frames;
FILE *file;
int64_t timestamp = -1, fileSize = -1;
struct Record { unsigned width, height, offset, size, hash; };
std::map<hd_textures::Key, Record> records;
std::map<hd_textures::Key, bool> reported;
uint32_t read32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
void closePack()
{
    if (file) std::fclose(file);
    file = nullptr; records.clear(); reported.clear();
}
bool openPack(const std::string& path)
{
#ifdef _WIN32
    /* Permit the preparer's atomic rename while this snapshot is in use. The
     * old handle stays valid until the next safe frame picks up the new file. */
    HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_RDONLY | _O_BINARY);
    if (fd < 0) { CloseHandle(handle); return false; }
    file = _fdopen(fd, "rb");
    if (!file) { _close(fd); return false; }
#else
    file = std::fopen(path.c_str(), "rb");
#endif
    uint8_t header[16];
    if (!file || std::fread(header, 1, 16, file) != 16 || std::memcmp(header, "ISTEX001", 8)) return false;
    const unsigned count = read32(header+8), total = read32(header+12);
    if (!count || count > 16384 || total != uint64_t(fileSize) || uint64_t(count)*36+16 > total) return false;
    for (unsigned i = 0; i < count; ++i) {
        uint8_t bytes[36];
        if (std::fread(bytes, 1, 36, file) != 36) return false;
        hd_textures::Key key{read32(bytes), read32(bytes+4), read32(bytes+8), read32(bytes+12)};
        Record r{read32(bytes+16), read32(bytes+20), read32(bytes+24), read32(bytes+28), read32(bytes+32)};
        if (key.format > 4 || key.size > 3 || !r.width || !r.height || r.width > 4096 || r.height > 4096 ||
            r.offset < 16+count*36 || r.offset > total || !r.size || r.size > total-r.offset ||
            r.size > 80*1024*1024 || !records.emplace(key, r).second) return false;
    }
    std::fprintf(stderr, "hd: loaded %s (%u texture keys, %lld compressed bytes)\n", path.c_str(), count, (long long)fileSize);
    return true;
}
}
extern "C" void portTexturePackRoot(const char *root) { directory = root ? root : ""; }
extern "C" void portTexturePackBundleRoot(const char *root) { bundledDirectory = root ? root : ""; }
extern "C" void portTexturePackSet(int mode) { requested.store(mode >= 0 && mode <= 2 ? mode : 0); }
extern "C" int portTexturePackGet(void) { return requested.load(); }
namespace hd_textures {
bool beginFrame()
{
    const int mode = requested.load();
    const bool changed = mode != current;
    if (!changed && ++frames % 60 != 0) return false;
    const char *name = mode == 2 ? "/4k.istex" : "/hd.istex";
    std::string path = directory + name;
    struct stat st{};
    bool exists = mode && !directory.empty() && stat(path.c_str(), &st) == 0;
    // A present user override owns the selection, even if it is invalid. Only
    // an absent override falls through to the read-only application bundle.
    if (mode && !exists && !bundledDirectory.empty()) {
        path = bundledDirectory + name;
        exists = stat(path.c_str(), &st) == 0;
    }
    std::error_code timeError;
    const auto written = exists ? std::filesystem::last_write_time(path, timeError) : std::filesystem::file_time_type{};
    const int64_t nextTime = exists && !timeError ? written.time_since_epoch().count() : -1;
    const int64_t nextSize = exists ? st.st_size : -1;
    if (!changed && path == currentPath && nextTime == timestamp && nextSize == fileSize) return false;
    closePack(); current = mode; currentPath = path; timestamp = nextTime; fileSize = nextSize;
    if (exists && !openPack(path)) { closePack(); std::fprintf(stderr, "hd: invalid pack %s; original textures retained\n", path.c_str()); }
    if (mode && !exists) std::fprintf(stderr, "hd: %s is not installed; original textures retained\n", path.c_str());
    std::fflush(stderr);
    return true;
}
bool lookup(const Tile& tile, Image& image)
{
    if (!file) return false;
    Key key;
    if (!identify(tile, key)) return false;
    auto match = records.find(key);
    if (match == records.end() && key.palette) {
        Key wildcard = key; wildcard.palette = 0;
        match = records.find(wildcard);
    }
    if (match == records.end()) {
        if (std::getenv("GE_HD_TRACE") && reported.emplace(key, false).second) {
            std::fprintf(stderr, "hd: miss %08X#%u#%u#%08X native=%ux%u stride=%u\n", key.texture, key.format, key.size, key.palette, tile.width, tile.height, tile.stride);
            if (std::getenv("GE_HD_DUMP")) {
                char name[80]; std::snprintf(name,sizeof(name),"hd-native-%08X-%08X.bin",key.texture,key.palette);
                if (FILE *out = std::fopen(name,"wb")) {
                    uint32_t header[] = {tile.width,tile.height,tile.stride,tile.format,tile.size,unsigned(tile.bytes),tile.paletteCount,key.texture};
                    std::fwrite(header,4,8,out);
                    std::fwrite(tile.palette,2,tile.paletteCount,out);
                    std::fwrite(tile.pixels,1,tile.bytes,out); std::fclose(out);
                }
            }
        }
        return false;
    }
    const Record& r = match->second;
    if (uint64_t(r.width)*tile.height != uint64_t(r.height)*tile.width || r.width < tile.width) return false;
    std::vector<uint8_t> compressed(r.size);
    if (std::fseek(file, r.offset, SEEK_SET) || std::fread(compressed.data(), 1, r.size, file) != r.size ||
        !unpack(compressed.data(), compressed.size(), r.width, r.height, r.hash, image.rgba)) {
        if (reported.emplace(key, false).second) std::fprintf(stderr, "hd: corrupt image %08X; original retained\n", key.texture);
        return false;
    }
    image.width = r.width; image.height = r.height;
    if (reported.emplace(key, true).second) std::fprintf(stderr, "hd: match %08X#%u#%u#%08X %ux%u -> %ux%u\n", key.texture, key.format, key.size, key.palette, tile.width, tile.height, r.width, r.height);
    return true;
}
}
#endif
