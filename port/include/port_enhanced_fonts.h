#ifndef PORT_ENHANCED_FONTS_H_C
#define PORT_ENHANCED_FONTS_H_C
/* Enhanced fonts for the menus (FORK-052, font styles plan FONT-3): the
 * install state of the player's Data/FontPacks/gepd folder and the in-game
 * download, which runs only when the player picks Download on the Enhanced
 * Fonts page. Implemented in port/fast3d/enhanced_fonts_download.cpp; builds
 * without the renderer get the weak fallbacks in libultra_shim.c. */
#ifdef __cplusplus
extern "C" {
#endif

enum PortEnhancedFontsState {
    PORT_EFONTS_MISSING,     /* no valid files */
    PORT_EFONTS_PARTIAL,     /* some of the pinned files */
    PORT_EFONTS_INSTALLED,   /* all of them */
    PORT_EFONTS_DOWNLOADING,
    PORT_EFONTS_FAILED,      /* the last download stopped; portEnhancedFontsError says why */
    PORT_EFONTS_UNSUPPORTED  /* this build cannot download (the files can still be copied in) */
};

int portEnhancedFontsState(void);
/* Files verified so far and the pinned total, for the progress line. */
void portEnhancedFontsProgress(int *done, int *total);
/* Why the last download failed, short enough for one menu line. */
const char *portEnhancedFontsError(void);
/* Starts the download on a worker thread; the player chose Download. */
void portEnhancedFontsDownload(void);
void portEnhancedFontsCancel(void);
/* Counts the installed files again (after a manual copy). */
void portEnhancedFontsRecount(void);
/* Deletes the installed files (the pinned ones and the source note). */
void portEnhancedFontsRemove(void);
/* 1 once after a download finishes with every file installed, so the menu
 * can switch Fonts to Enhanced; 0 otherwise. */
int portEnhancedFontsJustInstalled(void);
/* Line i (0-based) of the page's description, with a trailing newline, or
 * NULL past the last. */
const char *portEnhancedFontsInfoLine(int i);

/* ---- The menus' shared actions (port/src/port_enhanced_fonts.c) ---- */
/* Action 0's label follows the state (Download, Cancel Download, Retry
 * Download, Use Enhanced Fonts, Check Folder Again); action 1 is Remove. */
const char *portEnhancedFontsActionLabel(int action);
/* Action 0: starts or cancels the download, or switches Fonts to Enhanced. */
void portEnhancedFontsPrimaryAction(void);
/* Whether any pinned file is installed, so Remove has something to do. */
int portEnhancedFontsHasFiles(void);
/* Removes the files and, if Fonts was Enhanced, sets it back to Original. */
void portEnhancedFontsRemoveAndRevert(void);
/* Called once per frame: switches Fonts to Enhanced after a download that
 * the player started finishes with every file installed. */
void portEnhancedFontsFrame(void);
/* The Fonts row's value text for Enhanced: "(Get)" until installed. */
const char *portEnhancedFontsValueText(const char *text);

#ifdef __cplusplus
}
#endif
#endif
