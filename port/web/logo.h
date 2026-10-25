#ifndef PORT_WEB_LOGO_H
#define PORT_WEB_LOGO_H
#include <ultra64.h>

enum port_web_logo_kind { PORT_LOGO_BYTES, PORT_LOGO_WORDS, PORT_LOGO_VERTICES };
struct port_web_logo_block {
    void *data;
    u32 size;
    u32 offset;
    enum port_web_logo_kind kind;
};
extern const struct port_web_logo_block portWebLogoBlocks[];
extern const u32 portWebLogoBlockCount;
s32 portWebCopyLogoBlocks(const u8 *rom, u32 size,
                         const struct port_web_logo_block *blocks, u32 count);
void portWebLoadRarewareLogo(void);
#endif
