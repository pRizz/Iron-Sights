/* The Enhanced fonts' menu actions (FORK-052, font styles plan FONT-3),
 * shared by the Options folder (front.c) and the watch (port_watchpages.c).
 * The download itself is port/fast3d/enhanced_fonts_download.cpp. */
#include <ultra64.h>
#include "port_enhanced_fonts.h"

extern s32 portOptFind(const char *key);
extern s32 portOptGet(s32 i);
extern s32 portOptSetValue(s32 i, s32 v);

#define FONT_STYLE_ENHANCED 1

static void setFontStyle(s32 style)
{
    const s32 i = portOptFind("font_style");

    if (i >= 0)
    {
        portOptSetValue(i, style);
    }
}

const char *portEnhancedFontsActionLabel(int action)
{
    if (action != 0)
    {
        return "Remove Enhanced Fonts\n";
    }
    switch (portEnhancedFontsState())
    {
    case PORT_EFONTS_DOWNLOADING: return "Cancel Download\n";
    case PORT_EFONTS_FAILED:      return "Retry Download\n";
    case PORT_EFONTS_INSTALLED:   return "Use Enhanced Fonts\n";
    case PORT_EFONTS_UNSUPPORTED: return "Check Folder Again\n";
    default:                      return "Download\n";
    }
}

void portEnhancedFontsPrimaryAction(void)
{
    switch (portEnhancedFontsState())
    {
    case PORT_EFONTS_DOWNLOADING: portEnhancedFontsCancel(); break;
    case PORT_EFONTS_INSTALLED:   setFontStyle(FONT_STYLE_ENHANCED); break;
    case PORT_EFONTS_UNSUPPORTED: portEnhancedFontsRecount(); break;
    default:                      portEnhancedFontsDownload(); break;
    }
}

int portEnhancedFontsHasFiles(void)
{
    int done = 0, total = 0;

    if (portEnhancedFontsState() == PORT_EFONTS_DOWNLOADING)
    {
        return 0;
    }
    portEnhancedFontsProgress(&done, &total);
    return done > 0;
}

void portEnhancedFontsRemoveAndRevert(void)
{
    const s32 i = portOptFind("font_style");

    portEnhancedFontsRemove();
    if (i >= 0 && portOptGet(i) == FONT_STYLE_ENHANCED)
    {
        portOptSetValue(i, 0);
    }
}

void portEnhancedFontsFrame(void)
{
    if (portEnhancedFontsJustInstalled())
    {
        setFontStyle(FONT_STYLE_ENHANCED);
    }
}

const char *portEnhancedFontsValueText(const char *text)
{
    return portEnhancedFontsState() == PORT_EFONTS_INSTALLED ? text : "Enhanced (Get)\n";
}
