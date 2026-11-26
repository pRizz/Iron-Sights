/* Native dialogs and restart adapter. Codec/storage never include engine SDK headers. */
#include "save_backup.h"
#include "port_save_transfer.h"
#include "port_modal.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cerrno>
#include <algorithm>
#include <ctime>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <SDL_syswm.h>
#else
#include <unistd.h>
#include <fstream>
#include <string>
#include <sys/wait.h>
#include <vector>
#include "posix_dialog.h"
#if __has_include(<X11/Xlib.h>)
#include <SDL_syswm.h> /* the game window's X11 id, a dialog's parent */
#endif
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

#ifndef _WIN32
/* FORK-092: Linux dialogs (zenity, else kdialog) over the game. */
static void pumpEvents(void) { SDL_PumpEvents(); }

/* overGame: the game window still exists (not after shutdown). XWayland turns a
 * warped hidden cursor into a pointer lock on the game's window, so a dialog took
 * typing but never a click: a visible cursor ends the lock once pumpEvents sends
 * it, and input_pc.c hides it again on the next poll. The game window is the
 * dialog's parent where the tool honours it (zenity 3, kdialog; zenity 4
 * ignores --attach). */
static posix_dialog::Result dialog(std::vector<std::string> zenity, std::vector<std::string> kdialog, bool overGame = true)
{
    if (overGame) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
#if defined(SDL_VIDEO_DRIVER_X11)
        SDL_SysWMinfo native;
        SDL_VERSION(&native.version);
        SDL_Window *window = gfx_sdl_get_window();
        if (window && SDL_GetWindowWMInfo(window, &native) && native.subsystem == SDL_SYSWM_X11) {
            const std::string parent = std::to_string((unsigned long) native.info.x11.window);
            zenity.push_back("--attach=" + parent);
            kdialog.insert(kdialog.begin(), {"--attach", parent});
        }
#endif
    }
    return posix_dialog::run(zenity, kdialog, overGame ? pumpEvents : nullptr);
}

/* The folder of the last export, kept beside the data (not a setting, and not
 * in backups: save_backup.cpp's allowedPath). */
static fs::path folderFile() { return rootPath() / ".save-transfer-folder"; }

/* Where the pickers start: the last export's folder if it still exists, else home. */
static fs::path startFolder()
{
    std::ifstream in(folderFile());
    std::string line;
    std::error_code error;
    if (in && std::getline(in, line) && !line.empty() && fs::is_directory(fs::path(line), error)) return fs::path(line);
    const char *home = std::getenv("HOME");
    return home && *home ? fs::path(home) : fs::current_path(error);
}

static void rememberFolder(const fs::path &file)
{
    std::ofstream out(folderFile(), std::ios::trunc);
    if (out) out << fs::absolute(file).parent_path().string() << '\n';
}
#endif

static void message(const char *text, bool error = false, bool afterShutdown = false)
{
    std::fprintf(stderr, "ge: save transfer: %s\n", text);
    if (SDL_ShowSimpleMessageBox(error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION,
            "Iron Sights - Save transfer", text, afterShutdown ? nullptr : gfx_sdl_get_window()) == 0)
        return;
#ifndef _WIN32
    /* FORK-092: under WSLg SDL could not open its message box ("No message
     * system available"), and the player saw nothing. */
    std::fprintf(stderr, "ge: save transfer: SDL message box unavailable (%s); using zenity or kdialog\n", SDL_GetError());
    dialog({error ? "--error" : "--info", "--title=Iron Sights - Save transfer", "--no-markup", std::string("--text=") + text},
        {"--title", "Iron Sights - Save transfer", error ? "--error" : "--msgbox", text}, !afterShutdown);
#endif
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

static std::string exportFilename()
{
    const auto now = std::time(nullptr);
    const auto *maybeLocal = std::localtime(&now);
    char filename[80];
    if (!maybeLocal || !std::strftime(filename, sizeof(filename),
            "iron-sights-saves-%Y-%m-%dT%H-%M-%S.json", maybeLocal))
        throw std::runtime_error("Could not determine the local time for the backup filename.");
    return filename;
}

static fs::path chooseFile(bool exporting)
{
    const std::string filename = exporting ? exportFilename() : "";
#ifdef _WIN32
    wchar_t path[32768] = {};
    std::copy(filename.begin(), filename.end(), path);
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
    /* FORK-092: the pickers start in the last export's folder, else home, with a
     * full path: zenity 4 ignores the suggested name (4.0.1 opens the folder but
     * leaves the name empty), and the launch folder (an AppImage's mount, say)
     * is no place for a backup. */
    const fs::path folder = startFolder();
    std::vector<std::string> zenity = {"--file-selection", "--file-filter=*.json"};
    std::vector<std::string> kdialog;
    if (exporting) {
        const std::string suggested = (folder / filename).string();
        zenity.insert(zenity.end(), {"--save", "--confirm-overwrite", "--title=Export Save", "--filename=" + suggested});
        kdialog = {"--getsavefilename", suggested, "*.json"};
    } else {
        /* A file in the folder, not the folder: zenity 4 opened a folder's
         * parent with the folder selected. */
        zenity.insert(zenity.end(), {"--title=Import Save", "--filename=" + (folder / "iron-sights-saves.json").string()});
        kdialog = {"--getopenfilename", folder.string(), "*.json"};
    }
    const auto run = dialog(zenity, kdialog);
    if (!run.started) throw std::runtime_error("Install zenity or kdialog to choose save backup files.");
    const std::string line = posix_dialog::chosenLine(run);
    if (line.empty()) return {};
    if (!posix_dialog::accepted(run))
        std::fprintf(stderr, "ge: save transfer: the file picker %s %d after a choice; using it\n",
            WIFSIGNALED(run.status) ? "died of signal" : "exited",
            WIFSIGNALED(run.status) ? WTERMSIG(run.status) : WEXITSTATUS(run.status));
    fs::path chosen(line);
    /* As the Windows picker's default extension: Import lists only *.json, so a
     * backup saved as a bare name could not be found. */
    if (exporting && chosen.extension().empty()) chosen += ".json";
    return chosen;
#endif
}

static bool confirmImport()
{
    static const char question[] =
        "Replace all local campaign saves, profiles, settings and controller mappings with this backup?\n\n"
        "Iron Sights will restart to load the imported data. Export first to keep a copy of your current saves.";
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"}, {0, 1, "Import and restart"}};
    SDL_MessageBoxData box = {SDL_MESSAGEBOX_WARNING, gfx_sdl_get_window(), "Import Save", question,
        2, buttons, nullptr};
    int choice = 0;
    if (SDL_ShowMessageBox(&box, &choice) == 0) return choice == 1;
#ifndef _WIN32
    /* FORK-092: without SDL's box (WSLg) Import could never be confirmed. Only a
     * clean Yes imports; a failed or crashed dialog is Cancel. */
    std::fprintf(stderr, "ge: save transfer: SDL message box unavailable (%s); using zenity or kdialog\n", SDL_GetError());
    return posix_dialog::accepted(dialog(
        {"--question", "--title=Import Save", "--no-markup", "--ok-label=Import and restart", "--cancel-label=Cancel",
            std::string("--text=") + question},
        {"--title", "Import Save", "--yes-label", "Import and restart", "--no-label", "Cancel", "--warningyesno", question}));
#else
    return false;
#endif
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
#ifndef _WIN32
                rememberFolder(path); /* FORK-092: the pickers start there next time */
#endif
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
