/* Bounded decoder for the generator's RGBA packets. No game inflater state
 * is shared with the render thread. */
#ifndef PORT_ICON_DECODE_H
#define PORT_ICON_DECODE_H
static int portIconDecode(const unsigned char *src, unsigned int size,
                          unsigned char *dst, unsigned int bytes)
{
    unsigned int in = 0, out = 0, i, count, repeat, needed;
    while (in < size) {
        if (size-in < 2) return 0;
        count = src[in] | (src[in+1] << 8);
        repeat = count & 0x8000;
        count &= 0x7fff;
        in += 2;
        if (!count || count > (bytes-out)/4) return 0;
        needed = repeat ? 4 : count*4;
        if (needed > size-in) return 0;
        for (i = 0; i < count*4; i++) dst[out+i] = src[in+(repeat ? i%4 : i)];
        in += needed;
        out += count*4;
    }
    return out == bytes;
}
#endif
