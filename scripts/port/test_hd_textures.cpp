/* ROM-free identity, bounded decode, pack lifecycle and fallback regression. */
#include "port/fast3d/hd_textures.h"
#include "port/include/port_hd_textures.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <filesystem>
using namespace hd_textures;
static std::vector<uint8_t> pixels, compressed;
static Tile source(std::array<uint8_t,32>& data)
{
    for (unsigned i = 0; i < data.size(); ++i) data[i] = i;
    return {data.data(), data.size(), 8, 4, 8, 4, 1, nullptr, 0, false};
}
static void defaultWithoutPack(const std::string& root)
{
    // Arrange: fresh native settings, no HD files installed.
    assert(portTexturePackGet() == 1);
    portTexturePackRoot((root+"/missing").c_str());
    std::array<uint8_t,32> data;
    Tile tile = source(data);
    Image image;
    // Act / Assert: retain the HD preference while the caller renders stock.
    assert(beginFrame() && !lookup(tile,image));
    assert(portTexturePackGet() == 1);
}
static void identity()
{
    // Arrange: independently known Rice I8 fingerprint, including row order.
    std::array<uint8_t,32> data;
    Tile tile = source(data);
    Key key;
    // Act / Assert
    assert(identify(tile,key) && key.texture == 0x8d8e8f90 && key.palette == 0);
    tile.restoreSwizzle = true;
    std::array<uint8_t,32> original;
    for (unsigned y=0; y<4; ++y)
        for (unsigned x=0; x<8; ++x) original[y*8+x] = data[y*8+((y&1) ? x^4 : x)];
    Tile swapped{original.data(), original.size(), 8,4,8,4,1,nullptr,0,false};
    Key reference;
    assert(identify(swapped,reference) && identify(tile,key) && key == reference);
    tile.bytes--; assert(!identify(tile,key)); tile.bytes++;
    tile.stride = 7; assert(!identify(tile,key)); tile.stride = 8;
    tile.width = 3; assert(!identify(tile,key));
}
static void nativeWords()
{
    // Arrange: equivalent N64 bytes stored as this port's native u16s.
    std::array<uint16_t,16> data;
    for (unsigned i=0; i<data.size(); ++i) data[i] = (i*2 << 8) | (i*2+1);
    Tile tile{reinterpret_cast<uint8_t *>(data.data()),32,4,4,8,0,2,nullptr,0,false};
    Key key;
    // Act / Assert
    assert(identify(tile,key) && key.texture == 0x8d8e8f90);
    std::array<uint32_t,8> words;
    for (unsigned i=0; i<words.size(); ++i) words[i] = (i*4<<24) | ((i*4+1)<<16) | ((i*4+2)<<8) | (i*4+3);
    Tile rgba32{reinterpret_cast<uint8_t *>(words.data()),32,2,4,8,0,3,nullptr,0,false};
    assert(identify(rgba32,key) && key.texture == 0x8d8e8f90);
}
static void palettes()
{
    // Arrange: identical indices and different TLUT content.
    std::array<uint8_t,32> data;
    Tile tile = source(data);
    std::array<uint16_t,256> palette{};
    tile.format = 2; tile.palette = palette.data(); tile.paletteCount = 256;
    Key a,b;
    // Act / Assert
    assert(identify(tile,a)); palette[30] = 0x7fff;
    assert(identify(tile,b) && a.texture == b.texture && a.palette != b.palette);
    tile.paletteCount = 31; assert(!identify(tile,a));
}
static void decode()
{
    // Arrange: stored DEFLATE block with filter 0, preserving RGBA and alpha.
    std::vector<uint8_t> filtered;
    for (unsigned y=0; y<8; ++y) {
        filtered.push_back(0);
        for (unsigned x=0; x<16*4; ++x) { uint8_t v = (x*7+y*13)&255; pixels.push_back(v); filtered.push_back(v); }
    }
    const unsigned n = filtered.size();
    compressed = {1,uint8_t(n),uint8_t(n>>8),uint8_t(~n),uint8_t(~n>>8)};
    compressed.insert(compressed.end(),filtered.begin(),filtered.end());
    std::vector<uint8_t> decoded;
    // Act / Assert
    const auto hash = pixelHash(pixels.data(),pixels.size());
    assert(unpack(compressed.data(),compressed.size(),16,8,hash,decoded) && decoded == pixels);
    assert(!unpack(compressed.data(),compressed.size()-1,16,8,hash,decoded) && decoded.empty());
    assert(!unpack(compressed.data(),compressed.size(),16,8,hash+1,decoded) && decoded.empty());
    assert(!unpack(compressed.data(),compressed.size(),4097,8,hash,decoded));
}
static void write32(std::ofstream& f, uint32_t v)
{
    for (unsigned i=0; i<4; ++i) f.put(uint8_t(v>>(i*8)));
}
static void filters()
{
    // Arrange: independent PNG filter vectors for two RGBA pixels on two rows.
    const std::vector<uint8_t> expected{20,40,60,80,30,50,70,90,25,45,65,85,40,60,80,100};
    const uint8_t filtered[5][18] = {
        {0,20,40,60,80,30,50,70,90,0,25,45,65,85,40,60,80,100},
        {1,20,40,60,80,10,10,10,10,1,25,45,65,85,15,15,15,15},
        {2,20,40,60,80,30,50,70,90,2,5,5,5,5,10,10,10,10},
        {3,20,40,60,80,20,30,40,50,3,15,25,35,45,13,13,13,13},
        {4,20,40,60,80,10,10,10,10,4,5,5,5,5,10,10,10,10}
    };
    // Act / Assert
    for (const auto& rows : filtered) {
        std::vector<uint8_t> encoded{1,18,0,237,255}, rgba;
        encoded.insert(encoded.end(),rows,rows+18);
        assert(unpack(encoded.data(),encoded.size(),2,2,pixelHash(expected.data(),expected.size()),rgba));
        assert(rgba == expected);
        encoded[5] = 5;
        assert(!unpack(encoded.data(),encoded.size(),2,2,0,rgba) && rgba.empty());
    }
}
static void lifecycle(const std::string& root)
{
    // Arrange: one valid test-only pack, no ROM/artwork.
    std::filesystem::create_directories(root);
    std::ofstream f(root+"/hd.istex",std::ios::binary);
    f.write("ISTEX001",8);
    for (auto v : {1u,52u+unsigned(compressed.size()),0x8d8e8f90u,0u,4u,1u,16u,8u,52u,unsigned(compressed.size()),pixelHash(pixels.data(),pixels.size())}) write32(f,v);
    f.write(reinterpret_cast<const char *>(compressed.data()),compressed.size()); f.close();
    portTexturePackRoot(root.c_str());
    std::array<uint8_t,32> data; Tile tile = source(data); Image image;
    // A pack installed after default-mode fallback is picked up without a toggle.
    for (unsigned i=0;i<61;++i) beginFrame();
    assert(portTexturePackGet() == 1 && lookup(tile,image) && image.rgba == pixels);
    // Act / Assert: apply on the owning thread; absent 4K and Original fall back.
    portTexturePackSet(0); assert(beginFrame() && !lookup(tile,image) && portTexturePackGet() == 0);
    portTexturePackSet(1); assert(beginFrame() && lookup(tile,image) && image.rgba == pixels);
    portTexturePackSet(2); assert(beginFrame() && !lookup(tile,image));
    portTexturePackSet(1); assert(beginFrame() && lookup(tile,image));
    std::filesystem::remove(root+"/hd.istex");
    for (unsigned i=0;i<61;++i) beginFrame();
    assert(!lookup(tile,image));
    portTexturePackSet(99); assert(portTexturePackGet() == 0);
}
static uint32_t get32(const uint8_t *p)
{
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
static void verifyPack(const char *path)
{
    // Arrange: a prepared artist pack. Validate every payload using the actual
    // native decoder, including images not reached by the short gameplay audit.
    std::ifstream file(path,std::ios::binary);
    const std::vector<uint8_t> pack((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    assert(pack.size() >= 16 && get32(pack.data()+12) == pack.size());
    const unsigned count = get32(pack.data()+8);
    assert(count && count <= 16384 && pack.size() >= 16+size_t(count)*36);
    // Act / Assert
    for (unsigned i=0; i<count; ++i) {
        const uint8_t *entry = pack.data()+16+i*36;
        const unsigned offset = get32(entry+24), length = get32(entry+28);
        assert(offset <= pack.size() && length <= pack.size()-offset);
        std::vector<uint8_t> rgba;
        assert(unpack(pack.data()+offset,length,get32(entry+16),get32(entry+20),get32(entry+32),rgba));
    }
    std::printf("HD pack: all %u keyed payloads passed native lossless decode/checksum validation\n",count);
}
int main(int argc,char **argv)
{
    if (argc == 3 && std::string(argv[1]) == "--pack") { verifyPack(argv[2]); return 0; }
    assert(argc == 2);
    defaultWithoutPack(argv[1]); identity(); nativeWords(); palettes(); decode(); filters(); lifecycle(argv[1]);
    std::puts("HD textures: default HD, missing-pack fallback/recovery, explicit opt-out, identity and lossless decode passed");
}
