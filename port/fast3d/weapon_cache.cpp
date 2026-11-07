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
#elif defined(__linux__)
#include <csignal>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
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
extern "C" void portIconCacheLatch(void) { if (!active && SDL_AtomicGet(&loaded)) active=1; }
extern "C" const unsigned char *portIconPixels(int icon)
{ return portIconAvailable(icon) ? icons[icon].data() : nullptr; }
extern "C" const char *portIconCacheStatus(void)
{
    switch (SDL_AtomicGet(&cacheState)) {
    case kPreparing: return "Preparing in the background (weapon names until ready)";
    case kReady: return active ? "Ready - prepared locally from your ROM"
                               : "Ready - prepared locally from your ROM (shown from the next mission)";
    case kFailed: return "Weapon names (preparation failed; retry next launch)";
    case kNoFolder: return "Weapon names (cache folder unavailable)";
    case kUnavailable: return "Weapon names (cache unavailable; game remains playable)";
    case kUnsupported: return "Weapon names (native preparation available on Windows and Linux)";
    default: return "Weapon names (icons not prepared)";
    }
}
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
        /* Recovery (about 40 ms an image) runs on a thread while the next
         * model renders; each result is collected when the next image's
         * captures arrive, so at most one pair of captures waits. */
        static RecoverJob *job;
        static SDL_Thread *thread;
        auto collect=[&] {
            if (!job) return;
            if (thread) SDL_WaitThread(thread,nullptr);
            thread=nullptr;
            const int done=job->icon;
            icons[done]=std::move(job->out);
            delete job; job=nullptr;
            if (icons[done].empty()) { portConLogf("ge: icons: empty/clipped icon %d\n",done); std::exit(4); }
            completed++;
            portConLogf("ge: icons: prepared %d/%d (icon %d)\n",completed,PORT_ICON_IMAGES,done);
            writeFile("progress.bin",Bytes{(unsigned char)completed});
        };
        collect();
        job=new RecoverJob{std::move(black),std::move(white),{},w,h,icon};
        black.clear(); white.clear();
        thread=SDL_CreateThread(recoverJob,"icon-recover",job);
        if (!thread) recoverJob(job);  /* no thread: recover here */
        if (completed==PORT_ICON_IMAGES-1) collect();
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
/* The worker: an isolated run of this executable that renders each weapon
 * model and writes the cache (portIconWorkerCapture). maxfps=240 lets it run
 * as fast as the GPU draws (240, 500 and 1000 all measured the same);
 * uncapped (0) stalls on a display reporting 0 Hz, such as Xvfb. */
static const char kWorkerIni[]="[display]\nfullscreen=window\nvsync=0\nmaxfps=240\nmsaa=1\nwidescreen=0\nlow_res=0\nfiltering=bilinear\nfilter_2d=1\nrender_thread=0\nweapon_hud=0\nmission_timer=0\nalways_health=0\n[input]\nmouse=1\n";
/* Neutral input: in this mode the game skips the level intro itself
 * (bondview2.c), so nothing needs pressing. */
static const char kWorkerReplay[]="P 0 0 0 0\nP 3000 0 0 0\n";
/* Beside GE_ROM, GE_ICON_OUTPUT, GE_ICON_ROM_CRC and GE_INPUTPLAY; every other
 * GE_ variable of the player's environment is left out. */
static const char *const kWorkerEnv[][2]={{"GE_ICON_WORKER","1"},{"GE_STAGE","dam"},{"GE_STAGE_AFTER","1"},
    {"GE_WINDOW","2048x1536"},{"GE_NOINPUT","1"},{"GE_SEED","1"},{"GE_CHEATS","invuln,invis,guns"},
    {"GE_FRAMES","3000"},{"GE_WEAPON_ICON_EXPORT","1"}};
static bool writeWorkerFiles(const fs::path &root)
{
    return writeFile(root/"midas.ini",Bytes(kWorkerIni,kWorkerIni+sizeof kWorkerIni-1))
        && writeFile(root/"input.txt",Bytes(kWorkerReplay,kWorkerReplay+sizeof kWorkerReplay-1));
}
struct ChildFiles {
    fs::path dir;
    ~ChildFiles() { std::error_code e; if (!dir.empty()) fs::remove_all(dir,e); }
};
/* A worker gets 180 s; it normally needs about 6. */
constexpr Uint32 kWorkerTimeoutMs=180000;
/* Worker folders a launch that quit mid-preparation left behind; only called
 * under the cache lock, so no other launch is using one. */
static void removeStaleWorkers(const fs::path &folder)
{
    std::error_code error;
    for (auto it=fs::directory_iterator(folder,error);!error && it!=fs::directory_iterator();it.increment(error)) {
        const auto name=it->path().filename().u8string();
        if (it->is_directory(error) && name.rfind(u8"worker-",0)==0) {
            std::error_code ignored;
            fs::remove_all(it->path(),ignored);
        }
    }
}
static void keepLog(const fs::path &root, const fs::path &cache)
{
    std::error_code error;
    fs::copy_file(root/"Data"/"Logs"/"ge_direct.log",cache.parent_path()/"last-preparation.log",fs::copy_options::overwrite_existing,error);
}

#endif

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
struct CacheLock {
    HANDLE handle;
    bool owned=false;
    ~CacheLock() { if (owned) ReleaseMutex(handle); if (handle) CloseHandle(handle); }
};
/* Kills the worker when the game exits, even mid-preparation: the job's
 * only handle closes with this process. */
static HANDLE workerJob()
{
    static HANDLE job=[] {
        HANDLE handle=CreateJobObjectW(nullptr,nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (handle) SetInformationJobObject(handle,JobObjectExtendedLimitInformation,&limits,sizeof limits);
        return handle;
    }();
    return job;
}
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
static bool generate(const fs::path &cache, const char *rom)
{
    WCHAR module[32768]; DWORD n=GetModuleFileNameW(nullptr,module,32768);
    if (!n || n>=32768) return false;
    fs::path source(module), base=source.parent_path();
    ChildFiles files {cache.parent_path()/("worker-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount()))};
    fs::create_directory(files.dir);
    fs::copy_file(source,files.dir/"IronSights.exe");
    fs::copy_file(base/"SDL2.dll",files.dir/"SDL2.dll");
    if (!writeWorkerFiles(files.dir)) return false;
    /* Build a private environment, with diagnostic/launch overrides removed.
     * Never change the parent game's environment or copy the user's ROM. */
    std::vector<std::wstring> entries;
    LPWCH block=GetEnvironmentStringsW();
    if (!block) return false;
    for (const WCHAR *p=block;*p;p+=wcslen(p)+1) if (_wcsnicmp(p,L"GE_",3)) entries.emplace_back(p);
    FreeEnvironmentStringsW(block);
    auto set=[&](const std::wstring &key,const std::wstring &value) { entries.emplace_back(key+L"="+value); };
    set(L"GE_ROM",fs::u8path(rom).native());
    set(L"GE_ICON_OUTPUT",cache.native());
    WCHAR crc[16]; swprintf(crc,16,L"%08x",romFingerprint); set(L"GE_ICON_ROM_CRC",crc);
    set(L"GE_INPUTPLAY",(files.dir/"input.txt").native());
    for (auto &pair : kWorkerEnv) set(fs::u8path(pair[0]).native(),fs::u8path(pair[1]).native());
    std::sort(entries.begin(),entries.end(),[](auto &a,auto &b){return _wcsicmp(a.c_str(),b.c_str())<0;});
    std::vector<WCHAR> environment;
    for (auto &entry:entries) { environment.insert(environment.end(),entry.begin(),entry.end()); environment.push_back(0); }
    environment.push_back(0);
    STARTUPINFOW startup={sizeof(startup)};
    ChildProcess child;
    auto &process=child.value;
    std::wstring command=quoted(files.dir/"IronSights.exe");
    /* Below normal priority, so the game starting meanwhile keeps the CPU.
     * Not created suspended: a game quitting between creation and the job
     * assignment would leave a suspended worker forever, while a running one
     * finishes (GE_FRAMES) and exits on its own. */
    if (!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,
        CREATE_UNICODE_ENVIRONMENT|CREATE_NO_WINDOW|BELOW_NORMAL_PRIORITY_CLASS,
        environment.data(),files.dir.c_str(),&startup,&process)) return false;
    if (HANDLE job=workerJob()) AssignProcessToJobObject(job,process.hProcess);
    if (WaitForSingleObject(process.hProcess,kWorkerTimeoutMs)==WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess,4); WaitForSingleObject(process.hProcess,5000);
    }
    DWORD exit=4;
    GetExitCodeProcess(process.hProcess,&exit);
    keepLog(files.dir,cache);
    return exit==0;
}
#elif defined(__linux__)
/* The same binary runs as the worker, with GE_DATA_ROOT giving it a root of
 * its own, so nothing is copied and its bundled libraries resolve as for the
 * game. main.c appends "/IronSights" to GE_DATA_ROOT and then drops the last
 * path part as if it were the executable's name: the root is the folder. */
struct CacheLock {
    int fd=-1;
    ~CacheLock() { if (fd>=0) { flock(fd,LOCK_UN); close(fd); } }
};
struct ChildProcess {
    pid_t pid=-1;
    int status=0;
    bool running() {
        if (pid<0) return false;
        if (waitpid(pid,&status,WNOHANG)!=pid) return true;
        pid=-1; return false;
    }
    void stop() {
        if (pid<0) return;
        kill(pid,SIGTERM);
        for (int i=0;i<250 && running();i++) SDL_Delay(20);
        if (pid>=0) { kill(pid,SIGKILL); waitpid(pid,&status,0); pid=-1; status=-1; }
    }
    ~ChildProcess() { stop(); }
};
static bool generate(const fs::path &cache, const char *rom)
{
    char self[4096]; ssize_t n=readlink("/proc/self/exe",self,sizeof self-1);
    if (n<=0 || n>=(ssize_t)sizeof self-1) return false;
    self[n]='\0';
    ChildFiles files {cache.parent_path()/("worker-"+std::to_string(getpid())+"-"+std::to_string(SDL_GetTicks()))};
    const fs::path root=files.dir;
    fs::create_directories(root);
    if (!writeWorkerFiles(root)) return false;
    std::vector<std::string> entries;
    for (char **p=environ;*p;p++) if (std::strncmp(*p,"GE_",3)) entries.emplace_back(*p);
    auto set=[&](const char *key,const std::string &value) { entries.emplace_back(std::string(key)+"="+value); };
    set("GE_ROM",rom); set("GE_DATA_ROOT",utf8(files.dir));
    set("GE_ICON_OUTPUT",utf8(cache));
    char crc[16]; std::snprintf(crc,sizeof crc,"%08x",romFingerprint); set("GE_ICON_ROM_CRC",crc);
    set("GE_INPUTPLAY",utf8(root/"input.txt"));
    for (auto &pair : kWorkerEnv) set(pair[0],pair[1]);
    /* Everything the child needs is built before fork: after it, only
     * async-signal-safe calls. */
    std::vector<char *> environment;
    for (auto &entry:entries) environment.push_back(entry.data());
    environment.push_back(nullptr);
    char *argv[]={self,nullptr};
    const std::string dir=utf8(root);
    const pid_t parent=getpid();
    ChildProcess child;
    child.pid=fork();
    if (child.pid<0) return false;
    if (child.pid==0) {
        /* Dies with the thread that started it (which waits for it), so the
         * game quitting mid-preparation ends it too; and yields the CPU. */
        prctl(PR_SET_PDEATHSIG,SIGKILL);
        if (getppid()!=parent) _exit(127);
        if (nice(10)==-1) { /* a lower priority is only a courtesy */ }
        int null=open("/dev/null",O_RDWR);
        if (null>=0) { dup2(null,0); dup2(null,1); dup2(null,2); }
        if (chdir(dir.c_str())==0) execve(self,argv,environment.data());
        _exit(127);
    }
    for (Uint32 start=SDL_GetTicks();child.running();SDL_Delay(50)) {
        if (SDL_GetTicks()-start>kWorkerTimeoutMs) { child.stop(); break; }
    }
    const bool ok=child.pid<0 && child.status!=-1 && WIFEXITED(child.status) && WEXITSTATUS(child.status)==0;
    if (!ok) portConLogf("ge: icons: worker %s %d\n",WIFSIGNALED(child.status) ? "killed by signal" : "exited",
                         WIFSIGNALED(child.status) ? WTERMSIG(child.status) : WEXITSTATUS(child.status));
    keepLog(root,cache);
    return ok;
}
#endif

#if defined(_WIN32) || defined(__linux__)
/* Takes the per-user cache lock (simultaneous launches neither duplicate the
 * work nor race the atomic replacement), then reads the cache another launch
 * may have finished meanwhile, or runs the worker. */
static int prepare(const fs::path &folder, const fs::path &cache, const std::string &rom)
{
#ifdef _WIN32
    auto key=utf8(folder); auto hash=portIconCrc((const unsigned char*)key.data(),key.size());
    std::wstring name=L"Local\\IronSightsIcons-"+std::to_wstring(hash);
    CacheLock mutex{CreateMutexW(nullptr,FALSE,name.c_str())};
    if (!mutex.handle) return kFailed;
    DWORD acquired=WaitForSingleObject(mutex.handle,185000);
    if (acquired!=WAIT_OBJECT_0 && acquired!=WAIT_ABANDONED) return kFailed;
    mutex.owned=true;
#else
    /* A per-user lock file plays the Windows mutex's part; flock is
     * released if its holder dies. */
    CacheLock lock{open(utf8(folder/"weapon-icons.lock").c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0644)};
    if (lock.fd<0) return kFailed;
    bool held=false;
    for (int i=0;i<185000/50 && !(held=flock(lock.fd,LOCK_EX|LOCK_NB)==0);i++) SDL_Delay(50);
    if (!held) return kFailed;
#endif
    if (readCache(cache)) return kReady;
    removeStaleWorkers(folder);
    return generate(cache,rom.c_str()) && readCache(cache) ? kReady : kFailed;
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
        char *pref=SDL_GetPrefPath("IronSights","IronSights");
        if (!pref) { SDL_AtomicSet(&cacheState,kNoFolder); return; }
        fs::path folder=fs::u8path(pref);SDL_free(pref);
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
        if (diagnostic && !forced && !SDL_getenv("GE_ICON_CACHE_BACKGROUND")) return;
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
