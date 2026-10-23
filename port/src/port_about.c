#include <stddef.h>
#include "port_about.h"
extern int strncmp(const char *, const char *, size_t);
int portAboutUrlAllowed(const char *url)
{
    static const char prefix[] = "https://github.com/";
    const unsigned char *p = (const unsigned char *)url;
    if (!url || strncmp(url, prefix, sizeof(prefix)-1)) return 0;
    for (; *p; p++) if (*p <= 32 || *p == '"' || *p == '\\' || *p == '<' || *p == '>') return 0;
    return 1;
}


#ifndef GE_VIDEO
void portAboutOpen(void) {}
#endif
