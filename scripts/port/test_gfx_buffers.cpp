#include "port/fast3d/shader_text.h"
#include "port/fast3d/gfx_memory.h"
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
extern "C" void sysFatalError(const char *, ...) { throw std::runtime_error("Rejected"); }

template<class F> static void refused(F fn)
{
    try { fn(); assert(!"Expected rejection"); } catch (const std::runtime_error&) {}
}

int main()
{
    // Arrange/act/assert: exact-capacity text and rejection before overflow.
    struct { char text[16]; uint32_t guard=0xaabbccdd; } buffer{};
    size_t used=0;
    append_str(buffer.text, &used, "123456789012345");
    assert(used==15 && buffer.text[15]==0);
    refused([&]{ append_line(buffer.text, &used, ""); });
    refused([&]{ shader_appendf(buffer.text, 0, "%s", std::string(2048,'a').c_str()); });
    refused([&]{ shader_appendf(buffer.text, std::numeric_limits<size_t>::max(), "x"); });
    assert(buffer.guard==0xaabbccdd);
    used=0; used+=shader_appendf(buffer.text, used, "%s %d", "field", 42);
    append_line(buffer.text, &used, ""); assert(std::string(buffer.text)=="field 42\n");

    // Arrange/act/assert: an exactly eight-byte ambient object has no direction.
    struct Light { uint8_t bytes[16]; } light;
    auto ambient=std::make_unique<uint8_t[]>(8);
    std::memset(ambient.get(), 7, 8);
    gfx_copy_light(light, ambient.get(), true);
    for(unsigned i=0;i<16;i++) assert(light.bytes[i]==(i<8?7:0));
    assert(gfx_span_fits(127,1,128)); assert(!gfx_span_fits(127,2,128));
    assert(!gfx_span_fits(0,std::numeric_limits<size_t>::max(),128));
    std::puts("Shader capacity, ambient read and graphics span checks passed");
}
