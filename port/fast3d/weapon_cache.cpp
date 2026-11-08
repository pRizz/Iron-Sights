/* First-run model renders use this executable's renderer in an isolated child.
 * The player needs only the same SDL/OpenGL runtime already used by the game. */
#include <cmath> /* Must precede SDL's math.h include on Linux. */
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "port_icon_cache.h"
#include "port_modal.h"
#include "port_icon_cache_format.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "weapon_worker.h"
namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;
/* Never destroyed: a background preparation may still be writing it while a
 * quitting game runs its static destructors. */
static std::array<Bytes, PORT_ICON_COUNT> &icons=*new std::array<Bytes, PORT_ICON_COUNT>;
static unsigned int romFingerprint;
/* A missing cache is prepared on a background thread while the game starts.
 * icons is written only before loaded is set, and the HUD reads it only once
 * portIconCacheLatch has seen loaded at a mission start, so a cache that
 * finishes mid-mission appears from the next one. */
enum CacheState { kNotPrepared, kPreparing, kReady, kFailed, kNoFolder, kUnavailable, kUnsupported };
static SDL_atomic_t cacheState;  /* a CacheState, for About */
static SDL_atomic_t loaded;      /* icons holds a validated cache */
static int active;               /* latched: the HUD may use icons */
/* The in-process icon stage's state (see portIconStagePending below). */
enum StageState { kStageNone, kStagePending, kStageActive, kStageFinishing, kStageReturning };
static SDL_atomic_t stageState;
static fs::path stageCache;
static int stage() { return SDL_AtomicGet(&stageState); }
extern "C" void portConLogf(const char *, ...);

static std::string utf8(const fs::path &path)
{ auto s = path.u8string(); return std::string(reinterpret_cast<const char *>(s.data()),s.size()); }
static Bytes readFile(const fs::path &path, size_t max)
{
    Bytes bytes;
    SDL_RWops *file = SDL_RWFromFile(utf8(path).c_str(), "rb");
    if (!file) return bytes;
    Sint64 size = SDL_RWsize(file);
    if (size >= 0 && (uint64_t)size <= max) {
        bytes.resize((size_t)size);
        if (SDL_RWread(file, bytes.data(), 1, bytes.size()) != bytes.size()) bytes.clear();
    }
    SDL_RWclose(file);
    return bytes;
}
static bool writeFile(const fs::path &path, const Bytes &bytes)
{
    SDL_RWops *file = SDL_RWFromFile(utf8(path).c_str(), "wb");
    if (!file) return false;
    bool ok = SDL_RWwrite(file, bytes.data(), 1, bytes.size()) == bytes.size();
    return SDL_RWclose(file) == 0 && ok;
}
static void put32(Bytes &out, uint32_t n)
{ for (int i=0;i<4;i++) out.push_back((unsigned char)(n>>(8*i))); }

/* Transparent texels take their nearest visible neighbours' colour, so the HUD's
 * bilinear taps and mip averaging never pull black into a weapon's edge. Alpha
 * and every visible texel are unchanged; the cache file keeps the recovered bytes. */
static void bleedEdges(Bytes &rgba)
{
    const int w=PORT_ICON_WIDTH, h=PORT_ICON_HEIGHT;
    /* 0 = transparent, 1 = queued for this ring, 2 = has a colour */
    std::vector<unsigned char> state((size_t)w*h);
    std::vector<int> ring, nextRing;
    auto neighbours=[&](int i, auto &&visit) {
        const int x=i%w, y=i/w;
        if (x>0) visit(i-1);
        if (x<w-1) visit(i+1);
        if (y>0) visit(i-w);
        if (y<h-1) visit(i+w);
    };
    auto queue=[&](std::vector<int> &into, int i) {
        neighbours(i,[&](int n) { if (!state[n]) { state[n]=1; into.push_back(n); } });
    };
    for (int i=0;i<w*h;i++) if (rgba[(size_t)i*4+3]) state[i]=2;
    for (int i=0;i<w*h;i++) if (state[i]==2) queue(ring,i);
    /* 32 rings cover the mip levels a HUD-sized icon reaches. */
    for (int pass=0;pass<32 && !ring.empty();pass++) {
        for (int i : ring) {
            int sum[3]={0,0,0}, count=0;
            neighbours(i,[&](int n) {
                if (state[n]!=2) return;
                for (int c=0;c<3;c++) sum[c]+=rgba[(size_t)n*4+c];
                count++;
            });
            for (int c=0;c<3;c++) rgba[(size_t)i*4+c]=(unsigned char)((sum[c]+count/2)/count);
        }
        for (int i : ring) state[i]=2;
        nextRing.clear();
        for (int i : ring) queue(nextRing,i);
        ring.swap(nextRing);
    }
}

static bool readCache(const fs::path &path)
{
    auto bytes = readFile(path, PORT_ICON_COUNT*(PORT_ICON_BYTES+4)+32);
    if (!portIconCacheValidate(bytes.data(),bytes.size(),romFingerprint)) return false;
    std::array<Bytes, PORT_ICON_COUNT> decoded;
    size_t at = 32;
    for (int i=0;i<PORT_ICON_COUNT;i++) {
        if (at+4 > bytes.size()) return false;
        uint32_t size = portIconGet32(bytes.data()+at); at += 4;
        bool needed = portIconSlotStored(i);
        if (size != (needed ? PORT_ICON_BYTES : 0) || size > bytes.size()-at) return false;
        decoded[i].assign(bytes.begin()+at,bytes.begin()+at+size); at += size;
    }
    if (at != bytes.size()) return false;
    for (auto &image : decoded) if (!image.empty()) bleedEdges(image);
    icons.swap(decoded);
    SDL_AtomicSet(&loaded,1);
    return true;
}
static bool writeCache(const fs::path &path)
{
    Bytes bytes {'M','I','D','A','S','I','C','O'};
    put32(bytes,PORT_ICON_CACHE_VERSION); put32(bytes,romFingerprint);
    put32(bytes,PORT_ICON_WIDTH); put32(bytes,PORT_ICON_HEIGHT); put32(bytes,PORT_ICON_COUNT); put32(bytes,0);
    icons[0] = icons[1];
    for (auto &image : icons) { put32(bytes,(uint32_t)image.size()); bytes.insert(bytes.end(),image.begin(),image.end()); }
    uint32_t crc = portIconCrc(bytes.data()+32,bytes.size()-32);
    for (int i=0;i<4;i++) bytes[28+i] = (unsigned char)(crc>>(8*i));
    fs::path temp = path; temp += ".tmp";
    if (!writeFile(temp,bytes)) return false;
#ifdef _WIN32
    return MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error; fs::rename(temp,path,error); return !error;
#endif
}

extern "C" int portIconAvailable(int icon)
{ return active && icon >= 0 && icon < PORT_ICON_COUNT && !icons[icon].empty(); }
extern "C" int portIconCacheReady(void) { return active; }
extern "C" void portIconCacheLatch(void)
{
    if (active || !SDL_AtomicGet(&loaded)) return;
    active=1;
    portConLogf("ge: icons: the weapon strip uses icons from here on\n");
}
extern "C" const unsigned char *portIconPixels(int icon)
{ return portIconAvailable(icon) ? icons[icon].data() : nullptr; }
extern "C" const char *portIconCacheStatus(void)
{
    switch (SDL_AtomicGet(&cacheState)) {
    case kPreparing: return stage()!=kStageNone ? "Preparing before the title (weapon names until ready)"
                                                   : "Preparing in the background (weapon names until ready)";
    case kReady: return active ? "Ready - prepared locally from your ROM"
                               : "Ready - prepared locally from your ROM (shown from the next mission)";
    case kFailed: return "Weapon names (preparation failed; retry next launch)";
    case kNoFolder: return "Weapon names (cache folder unavailable)";
    case kUnavailable: return "Weapon names (cache unavailable; game remains playable)";
    case kUnsupported: return "Weapon names (native preparation available on Windows and Linux)";
    default: return "Weapon names (icons not prepared)";
    }
}
/* The in-process icon stage (the web build, which cannot start a worker;
 * natively with GE_ICON_STAGE=1): with no cache, the game itself plays a short
 * Dam stage in export mode behind the startup screen before the title, then
 * returns to the legal screen as an attract demo does (front.c). */
/* Captures arrive on the render thread (which may be its own), the rest on
 * the game thread: the capture only marks the stage finishing, and the game
 * thread's next export draw starts the fade (portIconStageTick). */
extern "C" int portIconStagePending(void) { return stage()==kStagePending; }
extern "C" int portIconStageActive(void) { return stage()==kStageActive || stage()==kStageFinishing; }
extern "C" int portIconStageBusy(void) { return stage()!=kStageNone; }
static Uint32 stageStart;
static void stageProgress(int completed);
static bool stageSeen;  /* an export draw ran: the stage itself has begun */
extern "C" void portIconStageBegin(void)
{
    if (!SDL_AtomicCAS(&stageState,kStagePending,kStageActive)) return;
    stageStart=SDL_GetTicks();
    stageProgress(0);
}
#ifdef __EMSCRIPTEN__
extern "C" void portWebStartupProgress(const char *label, unsigned completed, unsigned total);
extern "C" const char *portRootPath(const char *name);  /* main.c */
#endif
static void stageProgress(int completed)
{
#ifdef __EMSCRIPTEN__
    portWebStartupProgress("Preparing weapon icons",(unsigned)completed,PORT_ICON_IMAGES);
#else
    (void)completed;
#endif
}
/* The stage ends either way; the HUD keeps names when it failed. */
static void stageFinish(bool ok)
{
    SDL_AtomicSet(&cacheState,ok ? kReady : kFailed);
    if (ok) SDL_AtomicSet(&loaded,1);
    SDL_AtomicSet(&stageState,kStageFinishing);
    portConLogf("ge: icons: in-process preparation %s after %u ms\n",ok ? "ready" : "failed; weapon names stay",
                (unsigned)(SDL_GetTicks()-stageStart));
}
/* front.c, as the stage starts: its end then returns to the legal screen. */
extern "C" int portIconStageIsCurrent(void) { return stage()==kStageActive; }
/* front.c, each front-end tick: the front end only ticks again once the stage
 * is over, also when it ended before the images did (Bond killed, say), when
 * the HUD keeps names. */
extern "C" void portIconStageFrontTick(void)
{
    const int was=stage();
    if (was!=kStageActive && was!=kStageFinishing && was!=kStageReturning) return;
    if (was==kStageActive && !stageSeen) return;  /* a tick before the stage loaded */
    if (was==kStageActive) stageFinish(false);
    SDL_AtomicSet(&stageState,kStageNone);
#ifdef __EMSCRIPTEN__
    portWebStartupProgress("Starting GoldenEye",0,0);
#endif
}
extern "C" void ramromFadeToTitle(void);  /* ramromreplay.c: the attract demos' exit */
extern "C" void portIconStageTick(void)
{
    stageSeen=true;
    /* It normally needs a few seconds; never hold the startup screen longer. */
    if (stage()==kStageActive && SDL_GetTicks()-stageStart>60000) {
        portConLogf("ge: icons: in-process preparation timed out\n");
        stageFinish(false);
    }
    if (!SDL_AtomicCAS(&stageState,kStageFinishing,kStageReturning)) return;
    ramromFadeToTitle();
}
extern "C" int portIconWorkerEnabled(void)
{ return stage()==kStageActive || SDL_getenv("GE_ICON_WORKER") != nullptr; }

/* Native area resampling, in premultiplied alpha, avoids dark fringe pixels.
 * Coordinates describe the recovered supersampled model, with GL's bottom-up
 * row order converted before the crop. No GPU/vendor byte identity is assumed. */
static Bytes recover(const Bytes &black, const Bytes &white, int w, int h)
{
    Bytes rgba((size_t)w*h*4);
    int left=w, right=0, top=h, bottom=0;
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
        size_t src=((size_t)(h-y-1)*w+x)*3, dst=((size_t)y*w+x)*4;
        int diff[3]; for (int c=0;c<3;c++) diff[c]=(int)white[src+c]-black[src+c];
        std::sort(diff,diff+3);
        int alpha=std::clamp(255-diff[1],0,255); if (alpha<3) alpha=0;
        rgba[dst+3]=(unsigned char)alpha;
        for (int c=0;c<3;c++) rgba[dst+c] = alpha ? (unsigned char)std::clamp((black[src+c]*255+alpha/2)/alpha,0,255) : 0;
        if (alpha>8) { left=std::min(left,x);right=std::max(right,x+1);top=std::min(top,y);bottom=std::max(bottom,y+1); }
    }
    if (right<=left || bottom<=top || left<2 || top<2 || right>w-2 || bottom>h-2) return {};
    left=std::max(0,left-4); top=std::max(0,top-4); right=std::min(w,right+4); bottom=std::min(h,bottom+4);
    double scale=std::min(488.0/(right-left),144.0/(bottom-top));
    int dw=std::max(1,(int)std::floor((right-left)*scale)), dh=std::max(1,(int)std::floor((bottom-top)*scale));
    int ox=(PORT_ICON_WIDTH-dw)/2, oy=(PORT_ICON_HEIGHT-dh)/2;
    Bytes out(PORT_ICON_BYTES);
    for (int y=0;y<dh;y++) for (int x=0;x<dw;x++) {
        double x0=left+(double)x*(right-left)/dw, x1=left+(double)(x+1)*(right-left)/dw;
        double y0=top+(double)y*(bottom-top)/dh, y1=top+(double)(y+1)*(bottom-top)/dh;
        double sum[4]={0,0,0,0}, area=(x1-x0)*(y1-y0);
        for (int sy=(int)y0;sy<(int)std::ceil(y1);sy++) for (int sx=(int)x0;sx<(int)std::ceil(x1);sx++) {
            double weight=(std::min(x1,(double)sx+1)-std::max(x0,(double)sx))*(std::min(y1,(double)sy+1)-std::max(y0,(double)sy));
            size_t pos=((size_t)sy*w+sx)*4;
            double alpha=rgba[pos+3]*weight;
            for (int c=0;c<3;c++) sum[c]+=rgba[pos+c]*alpha;
            sum[3]+=alpha;
        }
        size_t pos=((size_t)(y+oy)*PORT_ICON_WIDTH+x+ox)*4;
        out[pos+3]=(unsigned char)std::clamp((int)std::lround(sum[3]/area),0,255);
        for (int c=0;c<3;c++) out[pos+c]=sum[3]>0 ? (unsigned char)std::clamp((int)std::lround(sum[c]/sum[3]),0,255) : 0;
    }
    return out;
}

/* One image's recovery, run on an SDL thread while the next model renders.
 * (SDL's C threads: <thread> pulls in <semaphore>, which this target's own
 * limits.h breaks on Linux.) */
struct RecoverJob {
    Bytes black, white, out;
    int w, h, icon;
};
static int SDLCALL recoverJob(void *data)
{
    auto *job=static_cast<RecoverJob *>(data);
    job->out=recover(job->black,job->white,job->w,job->h);
    return 0;
}

extern "C" void portIconWorkerCapture(int icon, int capture, int w, int h, const unsigned char *rgb)
{
    static Bytes black, white;
    static int last=-1, phase, completed;
    if (stage()==kStageFinishing || stage()==kStageReturning) return;  /* the last frames */
    const bool inProcess=stage()==kStageActive;
    /* A worker exits; the in-process stage ends and the game carries on. */
    auto fail=[&] { if (!inProcess) std::exit(4); stageFinish(false); };
    if (icon<1 || icon>=PORT_ICON_COUNT || w<64 || h<64 || w>4096 || h>4096 || !rgb) return fail();
    if (capture==1) { last=icon; phase=0; }
    if (icon!=last || capture!=++phase) return fail();
    size_t size=(size_t)w*h*3;
    if (capture==1) black.assign(rgb,rgb+size);
    if (capture==2) white.assign(rgb,rgb+size);
    if ((capture==3 && (black.size()!=size || std::memcmp(black.data(),rgb,size)))
        || (capture==4 && (white.size()!=size || std::memcmp(white.data(),rgb,size)))) {
        portConLogf("ge: icons: non-repeatable capture for icon %d\n",icon); return fail();
    }
    if (capture==4) {
        /* Recovery (about 40 ms an image) runs on a thread while the next
         * model renders; each result is collected when the next image's
         * captures arrive, so at most one pair of captures waits. */
        static RecoverJob *job;
        static SDL_Thread *thread;
        auto collect=[&]() -> bool {
            if (!job) return true;
            if (thread) SDL_WaitThread(thread,nullptr);
            thread=nullptr;
            const int done=job->icon;
            icons[done]=std::move(job->out);
            delete job; job=nullptr;
            if (icons[done].empty()) { portConLogf("ge: icons: empty/clipped icon %d\n",done); return false; }
            completed++;
            portConLogf("ge: icons: prepared %d/%d (icon %d)\n",completed,PORT_ICON_IMAGES,done);
            if (inProcess) stageProgress(completed);
            else writeFile("progress.bin",Bytes{(unsigned char)completed});
            return true;
        };
        if (!collect()) return fail();
        job=new RecoverJob{std::move(black),std::move(white),{},w,h,icon};
        black.clear(); white.clear();
        thread=SDL_CreateThread(recoverJob,"icon-recover",job);
        if (!thread) recoverJob(job);  /* no thread: recover here */
        if (completed==PORT_ICON_IMAGES-1 && !collect()) return fail();
        if (completed==PORT_ICON_IMAGES && inProcess) {
            const bool saved=writeCache(stageCache);
            portConLogf("ge: icons: cache write %s\n",saved ? "complete" : "failed");
            /* The images are in memory either way; unsaved, the next launch
             * prepares them again. */
            stageFinish(true);
            return;
        }
        if (completed==PORT_ICON_IMAGES) {
            const char *value=SDL_getenv("GE_ICON_OUTPUT");
            if (!value) std::exit(4);
            std::string output=value; /* SDL's Windows getenv buffer is reused. */
            const char *fingerprint=SDL_getenv("GE_ICON_ROM_CRC");
            if (!fingerprint) std::exit(4);
            romFingerprint=(unsigned int)std::strtoul(fingerprint,nullptr,16);
            bool saved=writeCache(fs::u8path(output));
            portConLogf("ge: icons: cache write %s\n",saved ? "complete" : "failed");
            std::exit(saved ? 0 : 4);
        }
    }
}


#if defined(_WIN32) || defined(__linux__)
/* Under the per-user cache lock (simultaneous launches neither duplicate the
 * work nor race the atomic replacement): reads the cache another launch may
 * have finished meanwhile, or runs the worker. */
static int prepare(const fs::path &folder, const fs::path &cache, const std::string &rom)
{
    int result=kFailed;
    weapon_worker::withLock(folder,[&] {
        if (readCache(cache)) { result=kReady; return; }
        weapon_worker::removeStale(folder);
        result=weapon_worker::generate(cache,rom.c_str(),romFingerprint) && readCache(cache) ? kReady : kFailed;
    });
    return result;
}
struct Preparation { fs::path folder, cache; std::string rom; };
static int SDLCALL prepareInBackground(void *data)
{
    std::unique_ptr<Preparation> job(static_cast<Preparation *>(data));
    int result=kUnavailable;
    try { result=prepare(job->folder,job->cache,job->rom); }
    catch (const std::exception &error) { portConLogf("ge: icons: %s\n",error.what()); }
    SDL_AtomicSet(&cacheState,result);
    portConLogf("ge: icons: background preparation %s\n",result==kReady ? "ready" : "failed; weapon names stay");
    return 0;
}
#endif

extern "C" void portIconCacheInit(const char *rom, unsigned int fingerprint)
{
    if (portIconWorkerEnabled()) return;
    romFingerprint=fingerprint;
    try {
#ifdef __EMSCRIPTEN__
        /* SDL's preference path is not persisted in the browser; the user
         * folder is (IndexedDB, web/src/persistent-files.ts). */
        fs::path folder=fs::u8path(portRootPath("Data/Cache"));
#else
        char *pref=SDL_GetPrefPath("IronSights","IronSights");
        if (!pref) { SDL_AtomicSet(&cacheState,kNoFolder); return; }
        fs::path folder=fs::u8path(pref);SDL_free(pref);
#endif
        if (const char *overridePath=SDL_getenv("GE_ICON_CACHE_DIR")) folder=fs::u8path(overridePath);
        fs::create_directories(folder);
        char name[80]; std::snprintf(name,sizeof(name),"weapon-icons-v%d-%08x.cache",PORT_ICON_CACHE_VERSION,fingerprint);
        fs::path cache=folder/name;
        if (readCache(cache)) {
            SDL_AtomicSet(&cacheState,kReady);
            portIconCacheLatch();
            portConLogf("ge: icons: reusing validated cache\n");
            return;
        }
        bool diagnostic=SDL_getenv("GE_FRAMES") || SDL_getenv("GE_STAGE") || SDL_getenv("GE_MENU") || SDL_getenv("GE_WEAPON_ICON_EXPORT");
        /* GE_ICON_CACHE_FORCE prepares in the foreground despite a scripted
         * run; GE_ICON_CACHE_BACKGROUND in the background, as a player's. */
        const bool forced=SDL_getenv("GE_ICON_CACHE_FORCE")!=nullptr;
#ifdef __EMSCRIPTEN__
        const bool inProcess=true;
#else
        const bool inProcess=SDL_getenv("GE_ICON_STAGE")!=nullptr;
#endif
        if (diagnostic && !forced && !inProcess && !SDL_getenv("GE_ICON_CACHE_BACKGROUND")) return;
        if (inProcess) {
            stageCache=cache;
            SDL_AtomicSet(&stageState,kStagePending);
            SDL_AtomicSet(&cacheState,kPreparing);
            portConLogf("ge: icons: preparing in process before the title\n");
            return;
        }
#if defined(_WIN32) || defined(__linux__)
        SDL_AtomicSet(&cacheState,kPreparing);
        if (forced) {
            /* Harnesses (the review gallery) need the cache before their
             * short run ends: prepare in the foreground. */
            PortModalScope modal;
            SDL_AtomicSet(&cacheState,prepare(folder,cache,rom));
            portIconCacheLatch();
        } else {
            auto *job=new Preparation{folder,cache,rom};
            SDL_Thread *thread=SDL_CreateThread(prepareInBackground,"icon-cache",job);
            if (thread) SDL_DetachThread(thread);
            else { delete job; SDL_AtomicSet(&cacheState,kFailed); }
        }
#else
        SDL_AtomicSet(&cacheState,kUnsupported);
#endif
    } catch (const std::exception &error) {
        SDL_AtomicSet(&cacheState,kUnavailable);
        portConLogf("ge: icons: %s\n",error.what());
    }
    portConLogf("ge: icons: %s\n",portIconCacheStatus());
}
