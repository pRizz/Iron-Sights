/* The weapon icon worker process (FORK-019): Windows and Linux run this
 * executable a second time to render the icons (weapon_cache.cpp's
 * portIconWorkerCapture), under a per-user lock. */
#if defined(_WIN32) || defined(__linux__)
#include <cmath> /* Must precede SDL's math.h include on Linux. */
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include "port_icon_cache_format.h"
#include "weapon_worker.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
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
extern "C" void portConLogf(const char *, ...);

namespace weapon_worker {
namespace {
std::string utf8(const fs::path &path)
{ auto s = path.u8string(); return std::string(reinterpret_cast<const char *>(s.data()),s.size()); }
bool writeFile(const fs::path &path, const Bytes &bytes)
{
    SDL_RWops *file = SDL_RWFromFile(utf8(path).c_str(), "wb");
    if (!file) return false;
    bool ok = SDL_RWwrite(file, bytes.data(), 1, bytes.size()) == bytes.size();
    return SDL_RWclose(file) == 0 && ok;
}
}

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
void removeStale(const fs::path &folder)
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
bool generate(const fs::path &cache, const char *rom, unsigned romFingerprint)
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
bool withLock(const fs::path &folder, const std::function<void()> &fn)
{
    auto key=utf8(folder); auto hash=portIconCrc((const unsigned char*)key.data(),key.size());
    std::wstring name=L"Local\\IronSightsIcons-"+std::to_wstring(hash);
    CacheLock mutex{CreateMutexW(nullptr,FALSE,name.c_str())};
    if (!mutex.handle) return false;
    DWORD acquired=WaitForSingleObject(mutex.handle,185000);
    if (acquired!=WAIT_OBJECT_0 && acquired!=WAIT_ABANDONED) return false;
    mutex.owned=true;
    fn();
    return true;
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
bool generate(const fs::path &cache, const char *rom, unsigned romFingerprint)
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
bool withLock(const fs::path &folder, const std::function<void()> &fn)
{
    /* A per-user lock file plays the Windows mutex's part; flock is
     * released if its holder dies. */
    CacheLock lock{open(utf8(folder/"weapon-icons.lock").c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0644)};
    if (lock.fd<0) return false;
    bool held=false;
    for (int i=0;i<185000/50 && !(held=flock(lock.fd,LOCK_EX|LOCK_NB)==0);i++) SDL_Delay(50);
    if (!held) return false;
    fn();
    return true;
}
#endif
}  // namespace weapon_worker
#endif
