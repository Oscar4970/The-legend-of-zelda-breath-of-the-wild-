#pragma once
// Verifies a triangle shader actually colored the center pixel while a corner
// remained unchanged after a Vulkan render pass and GPU->CPU readback.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
namespace triangleprobe {
constexpr std::uint32_t width=64,height=64;
constexpr std::array<int,4> triangle={230,51,26,255};
constexpr std::array<int,4> background={13,26,77,255};
inline bool matches(const std::uint8_t* pixels,std::size_t length,
                    std::uint32_t x,std::uint32_t y,
                    const std::array<int,4>& expected){
    if(!pixels || x>=width || y>=height || length<width*height*4u)return false;
    const auto at=(static_cast<std::size_t>(y)*width+x)*4;
    for(std::size_t i=0;i<4;i++)
        if(std::abs(static_cast<int>(pixels[at+i])-expected[i])>2)return false;
    return true;
}
inline bool verify(const std::uint8_t* pixels,std::size_t length){
    return matches(pixels,length,32,32,triangle) &&
           matches(pixels,length,3,3,background) &&
           matches(pixels,length,60,3,background);
}
}
