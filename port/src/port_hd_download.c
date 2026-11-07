/* The HD textures' menu actions (FORK-053), shared by the Options folder
 * (front.c) and the watch (port_watchpages.c). The download itself is
 * port/fast3d/hd_textures_download.cpp. */
#include <ultra64.h>
#include <stdio.h>
#include "port_about.h"
#include "port_hd_download.h"
#include "port_download_page.h"

extern int portTexturePackCustomCount(void);
extern const char *portTexturePackFolder(void);

extern s32 portOptFind(const char *key);
extern s32 portOptGet(s32 i);
extern s32 portOptSetValue(s32 i, s32 v);

#define TEXTURE_PACK_ORIGINAL 0
#define TEXTURE_PACK_HD       1

static void setTexturePack(s32 mode)
{
    const s32 i = portOptFind("texture_pack");

    if (i >= 0)
    {
        portOptSetValue(i, mode);
    }
}

/** Texture Pack is set to HD (a finished download sets it by itself). */
static s32 hdInUse(void)
{
    const s32 i = portOptFind("texture_pack");

    return i >= 0 && portOptGet(i) == TEXTURE_PACK_HD;
}

const char *portHdTexturesActionLabel(int action)
{
    if (action != 0)
    {
        return "Remove HD Textures\n";
    }
    switch (portHdTexturesState())
    {
    case PORT_HDTEX_DOWNLOADING: return "Cancel Download\n";
    case PORT_HDTEX_FAILED:      return "Retry Download\n";
    case PORT_HDTEX_INSTALLED:   return hdInUse() ? "Use Original Textures\n" : "Use HD Textures\n";
    case PORT_HDTEX_UNSUPPORTED: return "Check Folder Again\n";
    default:                     return "Download\n";
    }
}

void portHdTexturesPrimaryAction(void)
{
    switch (portHdTexturesState())
    {
    case PORT_HDTEX_DOWNLOADING: portHdTexturesCancel(); break;
    case PORT_HDTEX_INSTALLED:   setTexturePack(hdInUse() ? TEXTURE_PACK_ORIGINAL : TEXTURE_PACK_HD); break;
    case PORT_HDTEX_UNSUPPORTED: portHdTexturesRecount(); break;
    default:                     portHdTexturesDownload(); break;
    }
}

int portHdTexturesHasFiles(void)
{
    int done = 0, total = 0;

    if (portHdTexturesState() == PORT_HDTEX_DOWNLOADING)
    {
        return 0;
    }
    portHdTexturesProgress(&done, &total);
    return done > 0;
}

/* Removing leaves Texture Pack alone: HD without a pack draws the originals,
 * and a pack the player copied in themselves still works. */
static void removeFiles(void)
{
    portHdTexturesRemove();
}

void portHdTexturesFrame(void)
{
    if (portHdTexturesJustInstalled())
    {
        setTexturePack(TEXTURE_PACK_HD);
    }
}

int portHdTexturesOffered(void)
{
    const int state = portHdTexturesState();

    return state != PORT_HDTEX_INSTALLED && state != PORT_HDTEX_UNSUPPORTED;
}

/* CUSTOM TEXTURE PACKS: how to add a pack of one's own, how many textures
 * the game found in them, and a button that opens the folder. Lines fit the
 * watch face (about 29 characters). */
static const char *customInfoLine(int i)
{
    static char status[48];
    static const char *const lines[] = {
        "Put your own packs in\n",
        "Data/TexturePacks too:\n",
        "ZIPs or folders of\n",
        "GOLDENEYE#...png files.\n",
        "They win over HD Textures,\n",
        "and saved files show soon.\n",
        "Gun and hands stay original.\n",
    };
    const int fixed = (int) (sizeof(lines) / sizeof(lines[0]));
    int count;

    if (i >= 0 && i < fixed)
    {
        return lines[i];
    }
    if (i != fixed)
    {
        return NULL;
    }
    if (!hdInUse())
    {
        return "Texture Pack is Original.\n";
    }
    count = portTexturePackCustomCount();
    if (count < 0)
    {
        return "Looking for packs...\n";
    }
    if (count == 0)
    {
        return "No custom pack found.\n";
    }
    snprintf(status, sizeof(status), "Custom packs: %d textures\n", count);
    return status;
}

static const char *customActionLabel(int action)
{
    return action == 0 ? "Open Folder\n" : NULL;
}

/** Opens Data/TexturePacks (created if missing) in the system's file manager. */
static void customOpenFolder(void)
{
#if !defined(__EMSCRIPTEN__) && !defined(GE_XBOX)
    portOpenFolder(portTexturePackFolder());
#endif
}

static int customHasFiles(void)
{
    return 0;
}

static void customRemove(void)
{
}

const struct PortDownloadPage portCustomTexturesPage = {
    customInfoLine,
    customActionLabel,
    customOpenFolder,
    customHasFiles,
    customRemove,
    NULL,
    "@custom_textures_open",
    NULL,
};

const struct PortDownloadPage portHdTexturesPage = {
    portHdTexturesInfoLine,
    portHdTexturesActionLabel,
    portHdTexturesPrimaryAction,
    portHdTexturesHasFiles,
    removeFiles,
    "Remove HD Textures?\n",
    "@hd_textures_get",
    "@hd_textures_remove",
};
