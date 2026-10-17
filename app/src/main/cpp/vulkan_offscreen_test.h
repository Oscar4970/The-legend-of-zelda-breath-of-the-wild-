#pragma once
// A single Vulkan offscreen color clear is a GPU smoke test, not an NVN renderer.
// No game files, external code, or persistent graphics state.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
namespace offscreen {
constexpr std::uint32_t kWidth=16, kHeight=16;
constexpr std::array<int,4> kExpected = {64,128,191,255};
inline bool verifyRgba(const std::uint8_t* rgba,std::size_t bytes) {
    if(!rgba || bytes < kWidth*kHeight*4u)return false;
    // Test center pixel, all four channels, allow standard UNORM rounding.
    const auto at=(kHeight/2*kWidth+kWidth/2)*4u;
    for(std::size_t channel=0;channel<4;channel++) {
        if(std::abs(static_cast<int>(rgba[at+channel])-kExpected[channel])>2)
            return false;
    }
    return true;
}
inline const char* outcome(bool correct) {
    return correct?"GPU REAL: COLOR LEIDO CORRECTAMENTE":
                   "GPU: ERROR EN EL COLOR DEVUELTO";
}
} // namespace offscreen
