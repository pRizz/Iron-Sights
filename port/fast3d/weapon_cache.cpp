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
#include <string>
#include <vector>
#include "port_icon_cache.h"
#include "port_modal.h"
#include "port_icon_cache_format.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;
static std::array<Bytes, PORT_ICON_COUNT> icons;
static std::string cacheStatus = "Weapon names (icons not prepared)";
static unsigned int romFingerprint;
static bool ready;
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

static bool readCache(const fs::path &path)
{
    auto bytes = readFile(path, PORT_ICON_COUNT*(PORT_ICON_BYTES+4)+32);
    if (!portIconCacheValidate(bytes.data(),bytes.size(),romFingerprint)) return false;
    std::array<Bytes, PORT_ICON_COUNT> decoded;
    size_t at = 32;
    for (int i=0;i<PORT_ICON_COUNT;i++) {
        if (at+4 > bytes.size()) return false;
        uint32_t size = portIconGet32(bytes.data()+at); at += 4;
        bool needed = i != 23 && i != 30;
        if (size != (needed ? PORT_ICON_BYTES : 0) || size > bytes.size()-at) return false;
        decoded[i].assign(bytes.begin()+at,bytes.begin()+at+size); at += size;
    }
    if (at != bytes.size()) return false;
    icons.swap(decoded); ready = true;
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
{ return ready && icon >= 0 && icon < PORT_ICON_COUNT && !icons[icon].empty(); }
extern "C" int portIconCacheReady(void) { return ready; }
extern "C" const unsigned char *portIconPixels(int icon)
{ return portIconAvailable(icon) ? icons[icon].data() : nullptr; }
extern "C" const char *portIconCacheStatus(void) { return cacheStatus.c_str(); }
extern "C" int portIconWorkerEnabled(void) { return SDL_getenv("GE_ICON_WORKER") != nullptr; }

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

extern "C" void portIconWorkerCapture(int icon, int capture, int w, int h, const unsigned char *rgb)
{
    static Bytes black, white;
    static int last=-1, phase, completed;
    if (icon<1 || icon>=PORT_ICON_COUNT || w<64 || h<64 || w>4096 || h>4096 || !rgb) std::exit(4);
    if (capture==1) { last=icon; phase=0; }
    if (icon!=last || capture!=++phase) std::exit(4);
    size_t size=(size_t)w*h*3;
    if (capture==1) black.assign(rgb,rgb+size);
    if (capture==2) white.assign(rgb,rgb+size);
    if ((capture==3 && (black.size()!=size || std::memcmp(black.data(),rgb,size)))
        || (capture==4 && (white.size()!=size || std::memcmp(white.data(),rgb,size)))) {
        portConLogf("ge: icons: non-repeatable capture for icon %d\n",icon); std::exit(4);
    }
    if (capture==4) {
        icons[icon]=recover(black,white,w,h);
        if (icons[icon].empty()) { portConLogf("ge: icons: empty/clipped icon %d\n",icon); std::exit(4); }
        completed++;
        portConLogf("ge: icons: prepared %d/35 (icon %d)\n",completed,icon);
        writeFile("progress.bin",Bytes{(unsigned char)completed});
        if (completed==35) {
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

#ifdef _WIN32
static std::wstring quoted(const fs::path &path)
{
    std::wstring out=L"\"";
    /* A Windows path cannot contain a double quote. Double any trailing
     * backslashes so CreateProcess's argv decoder retains the closing quote. */
    auto value=path.native(); out+=value;
    for (auto it=value.rbegin();it!=value.rend() && *it==L'\\';++it) out+=L'\\';
    return out+L"\"";
}
struct ChildFiles {
    fs::path dir;
    ~ChildFiles() { std::error_code e; if (!dir.empty()) fs::remove_all(dir,e); }
};
struct CacheLock {
    HANDLE handle;
    bool owned=false;
    ~CacheLock() { if (owned) ReleaseMutex(handle); if (handle) CloseHandle(handle); }
};
struct ChildProcess {
    PROCESS_INFORMATION value={0};
    ~ChildProcess() {
        if (value.hProcess) {
            if (WaitForSingleObject(value.hProcess,0)==WAIT_TIMEOUT) {
                TerminateProcess(value.hProcess,4); WaitForSingleObject(value.hProcess,5000);
            }
            CloseHandle(value.hProcess);
        }
        if (value.hThread) CloseHandle(value.hThread);
    }
};
struct PreparationWindow {
    SDL_Window *window=nullptr;
    SDL_Renderer *renderer=nullptr;
    ~PreparationWindow() {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
};
static bool generate(const fs::path &cache, const char *rom)
{
    WCHAR module[32768]; DWORD n=GetModuleFileNameW(nullptr,module,32768);
    if (!n || n>=32768) return false;
    fs::path source(module), base=source.parent_path();
    ChildFiles files {cache.parent_path()/("worker-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount()))};
    fs::create_directory(files.dir);
    fs::copy_file(source,files.dir/"IronSights.exe");
    fs::copy_file(base/"SDL2.dll",files.dir/"SDL2.dll");
    std::string ini="[display]\nfullscreen=window\nvsync=0\nmaxfps=60\ntickhz=60\nmsaa=1\nwidescreen=0\nlow_res=0\nfiltering=bilinear\nfilter_2d=1\nrender_thread=0\nweapon_hud=0\nmission_timer=0\nalways_health=0\n[input]\nmouse=1\n";
    if (!writeFile(files.dir/"midas.ini",Bytes(ini.begin(),ini.end()))) return false;
    std::string replay="P 0 0 0 0\nP 80 8000 0 0\nP 82 0 0 0\nP 120 8000 0 0\nP 122 0 0 0\nP 160 8000 0 0\nP 162 0 0 0\nP 2000 0 0 0\n";
    if (!writeFile(files.dir/"input.txt",Bytes(replay.begin(),replay.end()))) return false;
    /* Build a private environment, with diagnostic/launch overrides removed.
     * Never change the parent game's environment or copy the user's ROM. */
    std::vector<std::wstring> entries;
    LPWCH block=GetEnvironmentStringsW();
    if (!block) return false;
    for (const WCHAR *p=block;*p;p+=wcslen(p)+1) if (_wcsnicmp(p,L"GE_",3)) entries.emplace_back(p);
    FreeEnvironmentStringsW(block);
    auto set=[&](const wchar_t *key,const std::wstring &value) { entries.emplace_back(std::wstring(key)+L"="+value); };
    set(L"GE_ROM",fs::u8path(rom).native()); set(L"GE_ICON_WORKER",L"1");
    set(L"GE_ICON_OUTPUT",cache.native());
    WCHAR crc[16]; swprintf(crc,16,L"%08x",romFingerprint); set(L"GE_ICON_ROM_CRC",crc);
    set(L"GE_STAGE",L"dam");set(L"GE_STAGE_AFTER",L"1");set(L"GE_WINDOW",L"2048x1536");
    set(L"GE_NOINPUT",L"1");set(L"GE_SEED",L"1");set(L"GE_CHEATS",L"invuln,invis,guns");
    set(L"GE_FRAMES",L"1800");set(L"GE_WEAPON_ICON_EXPORT",L"1");set(L"GE_INPUTPLAY",(files.dir/"input.txt").native());
    std::sort(entries.begin(),entries.end(),[](auto &a,auto &b){return _wcsicmp(a.c_str(),b.c_str())<0;});
    std::vector<WCHAR> environment;
    for (auto &entry:entries) { environment.insert(environment.end(),entry.begin(),entry.end()); environment.push_back(0); }
    environment.push_back(0);
    STARTUPINFOW startup={sizeof(startup)};
    ChildProcess child;
    auto &process=child.value;
    std::wstring command=quoted(files.dir/"IronSights.exe");
    if (!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_UNICODE_ENVIRONMENT|CREATE_NO_WINDOW,
        environment.data(),files.dir.c_str(),&startup,&process)) return false;
    PreparationWindow progressWindow;
    auto &window=progressWindow.window;
    auto &renderer=progressWindow.renderer;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO)==0) {
        window=SDL_CreateWindow("Iron Sights - Preparing weapon icons (first launch)",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,560,90,0);
        if (window) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    }
    DWORD start=GetTickCount(), exit=4;
    while (WaitForSingleObject(process.hProcess,20)==WAIT_TIMEOUT) {
        SDL_Event event; bool cancel=false;
        while (SDL_PollEvent(&event)) if (event.type==SDL_QUIT || (event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_ESCAPE)) cancel=true;
        if (cancel || GetTickCount()-start>180000) { TerminateProcess(process.hProcess,4); WaitForSingleObject(process.hProcess,5000); break; }
        auto progress=readFile(files.dir/"progress.bin",1); int count=progress.empty()?0:progress[0];
        if (window) {
            std::string title="Iron Sights - Preparing icons "+std::to_string(count)+"/35 (first launch; Esc to skip)";
            SDL_SetWindowTitle(window,title.c_str());
        }
        if (renderer) {
            SDL_SetRenderDrawColor(renderer,24,31,36,255);SDL_RenderClear(renderer);
            SDL_Rect outline={20,32,520,24}, bar={22,34,516*count/35,20};
            SDL_SetRenderDrawColor(renderer,110,130,140,255);SDL_RenderDrawRect(renderer,&outline);
            SDL_SetRenderDrawColor(renderer,183,219,144,255);SDL_RenderFillRect(renderer,&bar);SDL_RenderPresent(renderer);
        }
    }
    GetExitCodeProcess(process.hProcess,&exit);
    {
        std::error_code error;
        fs::copy_file(files.dir/"Data"/"Logs"/"ge_direct.log",cache.parent_path()/"last-preparation.log",fs::copy_options::overwrite_existing,error);
    }
    return exit==0;
}
#endif

extern "C" void portIconCacheInit(const char *rom, unsigned int fingerprint)
{
    if (portIconWorkerEnabled()) return;
    PortModalScope modal;
    romFingerprint=fingerprint;
    try {
        char *pref=SDL_GetPrefPath("IronSights","IronSights");
        if (!pref) { cacheStatus="Weapon names (cache folder unavailable)";return; }
        fs::path folder=fs::u8path(pref);SDL_free(pref);
        if (const char *overridePath=SDL_getenv("GE_ICON_CACHE_DIR")) folder=fs::u8path(overridePath);
        fs::create_directories(folder);
        char name[80]; std::snprintf(name,sizeof(name),"weapon-icons-v%d-%08x.cache",PORT_ICON_CACHE_VERSION,fingerprint);
        fs::path cache=folder/name;
        if (readCache(cache)) {
            cacheStatus="Ready - prepared locally from your ROM";
            portConLogf("ge: icons: reusing validated cache\n");
            return;
        }
        bool diagnostic=SDL_getenv("GE_FRAMES") || SDL_getenv("GE_STAGE") || SDL_getenv("GE_MENU") || SDL_getenv("GE_WEAPON_ICON_EXPORT");
        if (diagnostic && !SDL_getenv("GE_ICON_CACHE_FORCE")) return;
#ifdef _WIN32
        /* A per-user cache lock keeps simultaneous launches from duplicating
         * generation or racing an atomic cache replacement. */
        auto key=utf8(folder); auto hash=portIconCrc((const unsigned char*)key.data(),key.size());
        std::wstring lock=L"Local\\IronSightsIcons-"+std::to_wstring(hash);
        CacheLock mutex{CreateMutexW(nullptr,FALSE,lock.c_str())};
        if (!mutex.handle) return;
        DWORD acquired=WaitForSingleObject(mutex.handle,185000);
        if (acquired==WAIT_OBJECT_0 || acquired==WAIT_ABANDONED) {
            mutex.owned=true;
            if (readCache(cache) || (generate(cache,rom) && readCache(cache))) cacheStatus="Ready - prepared locally from your ROM";
            else cacheStatus="Weapon names (preparation skipped or failed; retry next launch)";
        }
#else
        cacheStatus="Weapon names (native preparation currently available on Windows)";
#endif
    } catch (const std::exception &error) {
        cacheStatus="Weapon names (cache unavailable; game remains playable)";
        portConLogf("ge: icons: %s\n",error.what());
    }
    portConLogf("ge: icons: %s\n",cacheStatus.c_str());
}
