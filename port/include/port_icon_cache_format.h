/* Bounded, portable cache codec. No ROM bytes are distributed with the game. */
#ifndef PORT_ICON_CACHE_FORMAT_H
#define PORT_ICON_CACHE_FORMAT_H
#include <stdint.h>
#include <stddef.h>
#define PORT_ICON_COUNT 38
#define PORT_ICON_WIDTH 512
#define PORT_ICON_HEIGHT 160
#define PORT_ICON_BYTES (PORT_ICON_WIDTH*PORT_ICON_HEIGHT*4)
#define PORT_ICON_CACHE_VERSION 1
static uint32_t portIconCrc(const unsigned char *data, size_t size)
{
    uint32_t crc = 0xffffffffu;
    size_t i;
    int bit;
    for (i = 0; i < size; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) crc = (crc>>1) ^ (0xedb88320u & (0u-(crc&1)));
    }
    return ~crc;
}
static uint32_t portIconGet32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static int portIconCacheValidate(const unsigned char *data, size_t size, uint32_t rom)
{
    static const unsigned char magic[8]={'M','I','D','A','S','I','C','O'};
    size_t at=32;
    int i;
    if (!data || size<32 || size>PORT_ICON_COUNT*(PORT_ICON_BYTES+4u)+32u) return 0;
    for (i=0;i<8;i++) if(data[i]!=magic[i]) return 0;
    if (portIconGet32(data+8)!=PORT_ICON_CACHE_VERSION || portIconGet32(data+12)!=rom
        || portIconGet32(data+16)!=PORT_ICON_WIDTH || portIconGet32(data+20)!=PORT_ICON_HEIGHT
        || portIconGet32(data+24)!=PORT_ICON_COUNT || portIconGet32(data+28)!=portIconCrc(data+32,size-32)) return 0;
    for (i=0;i<PORT_ICON_COUNT;i++) {
        uint32_t n;
        if (size-at<4) return 0;
        n=portIconGet32(data+at);at+=4;
        if (n!=(i==23 || i==30 ? 0 : PORT_ICON_BYTES) || n>size-at) return 0;
        at+=n;
    }
    return at==size;
}
#endif
