#include <stdio.h>
extern "C" {
void portStateResetInput(void);
void portVideoHoldOverKey(int);
const char *portIconCacheStatus(void);
}

#include <stddef.h>
#include <string>
#include "port_about.h"
#include "port_modal.h"
#include "port_buildinfo.h"
#ifdef GE_VIDEO
#include <SDL.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <commctrl.h>
#include <SDL_syswm.h>
extern "C" SDL_Window *gfx_sdl_get_window(void);
#endif
#endif

#if defined(GE_VIDEO) && defined(_WIN32)
static std::string escapeText(const char *value)
{
    std::string result;
    for (;*value;value++) {
        if (*value=='<') result+="&lt;";
        else if (*value=='>') result+="&gt;";
        else if (*value=='&') result+="&amp;";
        else result+=*value;
    }
    return result;
}
static HRESULT CALLBACK aboutLink(HWND window, UINT event, WPARAM w, LPARAM value, LONG_PTR data)
{
    if (event == TDN_HYPERLINK_CLICKED) {
        char url[1024];
        WideCharToMultiByte(CP_UTF8, 0, (const WCHAR *)value, -1, url, sizeof(url), NULL, NULL);
        if (portAboutUrlAllowed(url) && SDL_OpenURL(url) < 0)
            MessageBoxW(window, L"Windows could not open your web browser.", L"Iron Sights", MB_OK|MB_ICONINFORMATION);
    }
    return S_OK;
}
#endif

void portAboutOpen(void)
{
#ifdef GE_VIDEO
    PortModalScope modal;
    char text[2400];

    snprintf(text, sizeof(text), "Iron Sights - a fork of Project Midas\n\nBuilt at: %s\nBuild: %s\nCommit: %s%s\nBranch: %s\nBuilder: %s\n\nWeapon icons: %s\n\n"
        "Build details identify this executable. Verify the download's signed GitHub attestation for provenance."
#if !defined(__EMSCRIPTEN__) && !defined(GE_XBOX)
        "\n\nOptional HD textures: GoldenEye 007 HD by intermissionfb."
        "\nFont textures / technical assistance: GhostlyDark."
        "\nDam environment prototype; original artwork rights remain with its authors."
        "\nhttps://evilgames.eu/texture-packs/ge007-hd.htm"
        "\nTexture pack source: github.com/GhostlyDark/GoldenEye-007-HD"
        "\nDEFLATE decoder: puff by Mark Adler."
#endif
        ,
        PORT_BUILD_TIME, PORT_BUILD_STRING, PORT_BUILD_SHA, PORT_BUILD_DIRTY ? " (modified source)" : "",
        PORT_BUILD_BRANCH, PORT_BUILD_CI ? "GitHub Actions" : "Local build - no CI attestation", portIconCacheStatus());
#ifdef _WIN32
    {
        char links[6000];
        WCHAR wide[6000];
        TASKDIALOGCONFIG dialog = {0};
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&controls);
        snprintf(links, sizeof(links), "%s\n\n<a href=\"%s\">Download releases</a>%s%s%s%s%s%s%s%s%s",
            escapeText(text).c_str(), PORT_BUILD_DOWNLOADS_URL,
            PORT_BUILD_SOURCE_URL[0] ? "\n<a href=\"" : "", PORT_BUILD_SOURCE_URL,
            PORT_BUILD_SOURCE_URL[0] ? "\">View source commit</a>" : "",
            PORT_BUILD_RUN_URL[0] ? "\n<a href=\"" : "", PORT_BUILD_RUN_URL,
            PORT_BUILD_RUN_URL[0] ? "\">View GitHub Actions build</a>" : "",
            PORT_BUILD_RELEASE_URL[0] ? "\n<a href=\"" : "", PORT_BUILD_RELEASE_URL,
            PORT_BUILD_RELEASE_URL[0] ? "\">View this release and provenance</a>" : "");
        MultiByteToWideChar(CP_UTF8, 0, links, -1, wide, sizeof(wide)/sizeof(wide[0]));
        dialog.cbSize = sizeof(dialog);
        SDL_SysWMinfo native;
        SDL_VERSION(&native.version);
        SDL_Window *owner=gfx_sdl_get_window();
        if (owner && SDL_GetWindowWMInfo(owner,&native)) dialog.hwndParent=native.info.win.window;
        dialog.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
        dialog.dwCommonButtons = TDCBF_CLOSE_BUTTON;
        dialog.pszWindowTitle = L"About Iron Sights";
        dialog.pszMainInstruction = L"Iron Sights";
        dialog.pszContent = wide;
        dialog.pfCallback = aboutLink;
        if (FAILED(TaskDialogIndirect(&dialog, NULL, NULL, NULL)))
            MessageBoxW(NULL, wide, L"About Iron Sights", MB_OK|MB_ICONINFORMATION);
    }
#else
    {
        const SDL_MessageBoxButtonData buttons[] = {{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,"Close"},
            {0,1,"Source"},{0,2,"Build"},{0,3,"Releases"}};
        SDL_MessageBoxData box = {SDL_MESSAGEBOX_INFORMATION,NULL,"About Iron Sights",text,4,buttons,NULL};
        int choice;
        while (SDL_ShowMessageBox(&box, &choice) == 0 && choice > 0) {
            const char *url = choice == 1 ? PORT_BUILD_SOURCE_URL : choice == 2 ? PORT_BUILD_RUN_URL : PORT_BUILD_DOWNLOADS_URL;
            if (portAboutUrlAllowed(url)) SDL_OpenURL(url);
        }
    }
#endif
    /* The activation press must not leak into the menu when the dialog closes. */
    {

      portStateResetInput();
      portVideoHoldOverKey(SDL_SCANCODE_RETURN);
      portVideoHoldOverKey(SDL_SCANCODE_ESCAPE);
      portVideoHoldOverKey(SDL_SCANCODE_SPACE); }
#endif
}
