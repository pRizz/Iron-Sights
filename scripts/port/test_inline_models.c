/* Synthetic ROM model: two texture entries share one inline texel array. */
#include <assert.h>
#include <stdarg.h>
#include "../../port/src/romfile_model.c"
void portConLogf(const char *format, ...) { (void)format; }
void osSyncPrintf(const char *format, ...) {
    va_list args; va_start(args,format); vfprintf(stderr,format,args); va_end(args); abort();
}
static void big32(u8 *p, u32 value)
{ p[0]=value>>24; p[1]=value>>16; p[2]=value>>8; p[3]=value; }
static void fixture(u8 *data, int depth)
{
    int i;
    memset(data,0,192);
    for(i=0;i<2;i++) {
        big32(data+i*12,0x05000060);
        data[i*12+4]=4; data[i*12+5]=4; data[i*12+8]=depth;
    }
    data[25]=MODELNODE_OPCODE_DL;
    big32(data+28,0x05000030); /* node rodata */
    big32(data+48,0x05000050); /* one ENDDL */
    data[80]=0xb8;
    for(i=0;i<64;i++) data[96+i]=i+1;
}
int main(void)
{
    u8 bytes[192];
    int depth,i;
    for(depth=0;depth<=3;depth++) {
        unsigned length=8u<<depth;
        fixture(bytes,depth);
        assert(romfileModelConvert(bytes,sizeof(bytes),0,2,"inline fixture",NULL));
        for(i=0;i<length;i++) assert(bytes[96+i]==(i^3)+1);
        assert(bytes[0]==0x60 && bytes[3]==5);
        assert(bytes[80]==0 && bytes[83]==0xb8);
    }
    fixture(bytes,1);big32(bytes,0x050000c0);
    assert(!romfileModelConvert(bytes,sizeof(bytes),0,2,"bad bounds",NULL));
    fixture(bytes,4);
    assert(!romfileModelConvert(bytes,sizeof(bytes),0,2,"bad depth",NULL));
    return 0;
}
