#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "port_icon_cache_format.h"
#include "../../port/src/port_about.c"
static void put(unsigned char *p, uint32_t value)
{ int i;for(i=0;i<4;i++)p[i]=(unsigned char)(value>>(8*i)); }
int main(void)
{
    assert(portAboutUrlAllowed("https://github.com/pRizz/Project-Midas-ADS/releases"));
    assert(!portAboutUrlAllowed(NULL));
    assert(!portAboutUrlAllowed("http://github.com/pRizz/Project-Midas-ADS"));
    assert(!portAboutUrlAllowed("https://github.com.example.org/release"));
    assert(!portAboutUrlAllowed("https://github.com/release\nanything"));
    assert(!portAboutUrlAllowed("https://github.com/\"bad"));
    size_t size=32+PORT_ICON_COUNT*4u+36u*PORT_ICON_BYTES, at=32;
    unsigned char *bytes=calloc(size,1);
    int i;
    assert(bytes);memcpy(bytes,"MIDASICO",8);put(bytes+8,PORT_ICON_CACHE_VERSION);
    put(bytes+12,0xb6330846);put(bytes+16,512);put(bytes+20,160);put(bytes+24,38);
    for(i=0;i<38;i++){uint32_t n=(i==23||i==30)?0:PORT_ICON_BYTES;put(bytes+at,n);at+=4+n;}
    assert(at==size);put(bytes+28,portIconCrc(bytes+32,size-32));
    assert(portIconCacheValidate(bytes,size,0xb6330846));
    assert(!portIconCacheValidate(bytes,size,0xdeadbeef));
    assert(!portIconCacheValidate(bytes,size-1,0xb6330846));
    assert(!portIconCacheValidate(NULL,size,0xb6330846));
    bytes[size-1]^=1;assert(!portIconCacheValidate(bytes,size,0xb6330846));bytes[size-1]^=1;
    put(bytes+32,0xffffffff);put(bytes+28,portIconCrc(bytes+32,size-32));
    assert(!portIconCacheValidate(bytes,size,0xb6330846));
    free(bytes);return 0;
}
