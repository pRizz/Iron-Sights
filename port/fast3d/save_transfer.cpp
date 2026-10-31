/* Native dialogs and restart adapter. Codec/storage never include engine SDK headers. */
#include "save_backup.h"
#include "port_save_transfer.h"
#include "port_modal.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cerrno>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <SDL_syswm.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

extern "C" {
const char *portRootPath(const char *name);
SDL_Window *gfx_sdl_get_window(void);
void portRequestQuit(void);
void portStateResetInput(void);
void portVideoHoldOverKey(int key);
void portOptSaveHold(int on);
void portProfileAutoSave(int flush);
}

namespace fs = std::filesystem;
static std::vector<std::string> launchArgs;
#ifdef _WIN32
static std::wstring launchCommand;
#endif
static bool restarting;

static fs::path rootPath() { return fs::path(portRootPath("")); }

static void message(const char *text, bool error = false, bool afterShutdown = false)
{
    std::fprintf(stderr, "ge: save transfer: %s\n", text);
    SDL_ShowSimpleMessageBox(error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION,
        "Iron Sights - Save transfer", text, afterShutdown ? nullptr : gfx_sdl_get_window());
}

static void restartAfterShutdown()
{
    if (!restarting) return;
    std::vector<const char *> args;
    for (const auto &arg : launchArgs) args.push_back(arg.c_str());
    args.push_back(nullptr);
#ifdef _WIN32
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (CreateProcessW(nullptr, launchCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return;
    }
#else
    execv(args[0], const_cast<char *const *>(args.data()));
#endif
    message("The backup is ready. Please reopen Iron Sights to finish importing it.", true, true);
}

int portSaveTransferStartup(int argc, char **argv)
{
    try {
        launchArgs.assign(argv, argv + argc);
#ifdef _WIN32
        launchCommand = GetCommandLineW(); // Preserve quoted ROM paths and original arguments.
#else
        char exe[4096];
        auto count = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (count <= 0) throw std::runtime_error("Cannot locate Iron Sights for restart.");
        exe[count] = 0;
        launchArgs[0] = exe;
#endif
        // Registered before video/config savers: runs after their exit handlers.
        std::atexit(restartAfterShutdown);
        savebackup::applyPending(rootPath());
        return 1;
    } catch (const std::exception &error) {
        message(error.what(), true);
        return 0;
    }
}

static fs::path chooseFile(bool exporting)
{
#ifdef _WIN32
    wchar_t path[32768] = L"iron-sights-saves.json";
    if (!exporting) path[0] = 0;
    OPENFILENAMEW picker = {};
    picker.lStructSize = sizeof(picker);
    SDL_SysWMinfo native;
    SDL_VERSION(&native.version);
    if (SDL_GetWindowWMInfo(gfx_sdl_get_window(), &native)) picker.hwndOwner = native.info.win.window;
    picker.lpstrFilter = L"Iron Sights save backup (*.json)\0*.json\0All files\0*.*\0\0";
    picker.lpstrFile = path;
    picker.nMaxFile = sizeof(path) / sizeof(path[0]);
    picker.lpstrDefExt = L"json";
    picker.lpstrTitle = exporting ? L"Export Save" : L"Import Save";
    picker.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_EXPLORER
        | (exporting ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (exporting ? GetSaveFileNameW(&picker) : GetOpenFileNameW(&picker)) return fs::path(path);
    if (CommDlgExtendedError()) throw std::runtime_error("Windows could not open the file picker.");
    return {};
#else
    // Exec argv directly: file paths and labels never become shell commands.
    int pipes[2];
    if (pipe(pipes)) throw std::runtime_error("Could not open the file picker.");
    pid_t child = fork();
    if (child == 0) {
        close(pipes[0]); dup2(pipes[1], STDOUT_FILENO); close(pipes[1]);
        if (exporting)
            execlp("zenity", "zenity", "--file-selection", "--save", "--confirm-overwrite",
                "--title=Export Save", "--filename=iron-sights-saves.json", "--file-filter=*.json", nullptr);
        else execlp("zenity", "zenity", "--file-selection", "--title=Import Save", "--file-filter=*.json", nullptr);
        if (exporting)
            execlp("kdialog", "kdialog", "--getsavefilename", "iron-sights-saves.json", "*.json", nullptr);
        else execlp("kdialog", "kdialog", "--getopenfilename", ".", "*.json", nullptr);
        _exit(127);
    }
    close(pipes[1]);
    if (child < 0) { close(pipes[0]); throw std::runtime_error("Could not start the file picker."); }
    std::string path;
    char buffer[1024];
    ssize_t count;
    while ((count = ::read(pipes[0], buffer, sizeof(buffer))) > 0) path.append(buffer, count);
    close(pipes[0]);
    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
        throw std::runtime_error("Install zenity or kdialog to choose save backup files.");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return {};
    if (!path.empty() && path.back() == '\n') path.pop_back();
    return fs::path(path);
#endif
}

static bool confirmImport()
{
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"}, {0, 1, "Import and restart"}};
    SDL_MessageBoxData box = {SDL_MESSAGEBOX_WARNING, gfx_sdl_get_window(), "Import Save",
        "Replace all local campaign saves, profiles, settings and controller mappings with this backup?\n\n"
        "Iron Sights will restart to load the imported data. Export first to keep a copy of your current saves.",
        2, buttons, nullptr};
    int choice = 0;
    return SDL_ShowMessageBox(&box, &choice) == 0 && choice == 1;
}

void portSaveTransferRequest(int exporting)
{
    PortModalScope modal;
    try {
        const auto path = chooseFile(exporting != 0);
        if (!path.empty()) {
            if (exporting) {
                portOptSaveHold(0);
                portProfileAutoSave(1);
                savebackup::write(path, savebackup::encode(savebackup::collect(rootPath())));
                message("Save backup exported. Import this JSON file in desktop or web Iron Sights.");
            } else {
                const auto text = savebackup::read(path);
                savebackup::decode(text);
                if (confirmImport()) {
                    savebackup::stage(rootPath(), text);
                    restarting = true;
                    portRequestQuit();
                }
            }
        }
    } catch (const std::exception &error) { message(error.what(), true); }
    portStateResetInput();
    for (int key : {SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_SPACE}) portVideoHoldOverKey(key);
}
