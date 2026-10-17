#include "../app/src/main/cpp/vulkan_triangle_verify.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
int main() {
    std::array<std::uint8_t,triangleprobe::width*triangleprobe::height*4> pixels{};
    assert(!triangleprobe::verify(pixels.data(),pixels.size()));
    std::cout<<"PASS: an image without drawn fragments cannot pass\n";
    for(std::size_t i=0;i<pixels.size();i+=4)
        for(int c=0;c<4;c++) pixels[i+c]=static_cast<std::uint8_t>(triangleprobe::background[c]);
    assert(!triangleprobe::verify(pixels.data(),pixels.size()));
    std::cout<<"PASS: clear-only render does not pass triangle shader verification\n";
    const auto center=(32*triangleprobe::width+32)*4;
    for(int i=0;i<4;i++)pixels[center+i]=static_cast<std::uint8_t>(triangleprobe::triangle[i]);
    assert(triangleprobe::verify(pixels.data(),pixels.size()));
    std::cout<<"PASS: expected SPIR-V triangle center and background accepted\n";
    pixels[center]=0;
    assert(!triangleprobe::verify(pixels.data(),pixels.size()));
    std::cout<<"PASS: wrong triangle color rejected\n";
    pixels[center]=230;
    pixels[(3*triangleprobe::width+3)*4]=220;
    assert(!triangleprobe::verify(pixels.data(),pixels.size()));
    std::cout<<"PASS: corner not cleared rejected\n";
    pixels[(3*triangleprobe::width+3)*4]=13;
    pixels[(3*triangleprobe::width+60)*4]=220;
    assert(!triangleprobe::verify(pixels.data(),pixels.size()));
    std::cout<<"PASS: independent outside pixel rejected\n";
    pixels[(3*triangleprobe::width+60)*4]=13;
    assert(!triangleprobe::verify(pixels.data(),pixels.size()-1));
    std::cout<<"PASS: truncated GPU readback rejected\n";
    assert(!triangleprobe::verify(nullptr,pixels.size()));
    std::cout<<"PASS: null GPU readback rejected\n";
    std::cout<<"All 8 SPIR-V triangle readback acceptance tests passed (GPU execution requires Android device).\n";
}
