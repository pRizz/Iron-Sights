#ifndef PORT_HD_DOWNLOAD_H
#define PORT_HD_DOWNLOAD_H
/* The HD textures' download for the menus (FORK-053): intermissionfb's
 * GoldenEye 007 HD pack, fetched from its makers' GitHub only when the
 * player picks Download on the HD Textures page, into Data/TexturePacks.
 * Implemented in port/fast3d/hd_textures_download.cpp (native builds); the
 * others get the weak fallbacks in libultra_shim.c, which say UNSUPPORTED. */
#ifdef __cplusplus
extern "C" {
#endif

enum PortHdTexturesState {
    PORT_HDTEX_MISSING,     /* none of the pinned files */
    PORT_HDTEX_PARTIAL,     /* some of them */
    PORT_HDTEX_INSTALLED,   /* all of them */
    PORT_HDTEX_DOWNLOADING,
    PORT_HDTEX_FAILED,      /* the last download stopped; portHdTexturesError says why */
    PORT_HDTEX_UNSUPPORTED  /* this build cannot download (a pack can still be copied in) */
};

int portHdTexturesState(void);
/* Where the files go: Data/TexturePacks under the game's root folder. */
void portHdTexturesSetDirectory(const char *directory);
/* Megabytes in place so far and in all, for the progress line. */
void portHdTexturesProgress(int *done_mb, int *total_mb);
/* Why the last download failed, short enough for one menu line. */
const char *portHdTexturesError(void);
/* Starts the download on a worker thread; the player chose Download. */
void portHdTexturesDownload(void);
void portHdTexturesCancel(void);
/* Looks at the folder again (after a manual copy). */
void portHdTexturesRecount(void);
/* Deletes the downloaded files (the pinned ones and the source note). */
void portHdTexturesRemove(void);
/* 1 once after a download finishes with every file in place; 0 otherwise. */
int portHdTexturesJustInstalled(void);
/* Line i (0-based) of the page's description, with a trailing newline, or
 * NULL past the last. */
const char *portHdTexturesInfoLine(int i);

/* ---- The menus' shared actions (port/src/port_hd_download.c) ---- */
/* Action 0 follows the state (Download, Cancel Download, Retry Download, Use
 * HD Textures / Use Original Textures, Check Folder Again); 1 is Remove. */
const char *portHdTexturesActionLabel(int action);
void portHdTexturesPrimaryAction(void);
/* Whether any pinned file is in place, so Remove has something to do. */
int portHdTexturesHasFiles(void);
/* Called once per frame: sets Texture Pack to HD after a download that the
 * player started finishes. */
void portHdTexturesFrame(void);
/* Whether Options offers GET HD TEXTURES: this build can download and no
 * pack is in use yet. */
int portHdTexturesOffered(void);

#ifdef __cplusplus
}
#endif
#endif
