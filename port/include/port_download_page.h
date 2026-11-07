#ifndef PORT_DOWNLOAD_PAGE_H
#define PORT_DOWNLOAD_PAGE_H
/* A download page's text and actions (Enhanced Fonts, FORK-052; HD Textures
 * and the Custom Texture Packs help page, FORK-053), so the Options folder
 * (front.c) and the watch (port_watchpages.c) draw and drive them with one set
 * of code. A page without a Remove returns NULL for action 1. */

struct PortDownloadPage {
    /* Line i of the description, then the status line; NULL past the last. */
    const char *(*info_line)(int i);
    /* Action 0 follows the state (Download, Cancel, Retry, Use ...); 1 is Remove. */
    const char *(*action_label)(int action);
    void (*primary_action)(void);
    /* Whether Remove has something to delete. */
    int (*has_files)(void);
    /* Deletes the files, setting the page's option back where needed. */
    void (*remove)(void);
    const char *remove_prompt;  /* the folder's Yes/No question */
    const char *help_get;       /* port_option_help.c keys for the two buttons */
    const char *help_remove;
};

extern const struct PortDownloadPage portEnhancedFontsPage;  /* port_enhanced_fonts.c */
extern const struct PortDownloadPage portHdTexturesPage;     /* port_hd_download.c */
extern const struct PortDownloadPage portCustomTexturesPage; /* port_hd_download.c */

#endif
