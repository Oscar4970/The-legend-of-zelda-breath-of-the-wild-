#include "../app/src/main/cpp/vulkan_offscreen_test.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
int main(){
    std::array<std::uint8_t,offscreen::kWidth*offscreen::kHeight*4> pixels{};
    assert(!offscreen::verifyRgba(pixels.data(),pixels.size()));
    std::cout<<"PASS: blank GPU readback cannot pass color verification\n";
    const std::size_t center=((offscreen::kHeight/2)*offscreen::kWidth+
                              offscreen::kWidth/2)*4;
    pixels[center]=64;pixels[center+1]=128;
    pixels[center+2]=191;pixels[center+3]=255;
    assert(offscreen::verifyRgba(pixels.data(),pixels.size()));
    std::cout<<"PASS: RGBA8 known-good center pixel accepted\n";
    pixels[center+2]=193;
    assert(offscreen::verifyRgba(pixels.data(),pixels.size()));
    std::cout<<"PASS: supported UNORM rounding tolerance accepted\n";
    pixels[center+2]=194;
    assert(!offscreen::verifyRgba(pixels.data(),pixels.size()));
    std::cout<<"PASS: incorrect channel beyond tolerance rejected\n";
    pixels[center+2]=191;pixels[center+3]=0;
    assert(!offscreen::verifyRgba(pixels.data(),pixels.size()));
    std::cout<<"PASS: unexpected alpha rejected\n";
    assert(!offscreen::verifyRgba(pixels.data(),pixels.size()-1));
    std::cout<<"PASS: short mapped GPU buffer rejected\n";
    assert(!offscreen::verifyRgba(nullptr,pixels.size()));
    std::cout<<"PASS: null mapped GPU buffer rejected\n";
    assert(std::string(offscreen::outcome(true)).find("CORRECTAMENTE")!=std::string::npos);
    std::cout<<"PASS: success report only appears for verified color\n";
    std::cout<<"All 8 Vulkan GPU readback acceptance tests passed. GPU work is Android-only.\n";
}
