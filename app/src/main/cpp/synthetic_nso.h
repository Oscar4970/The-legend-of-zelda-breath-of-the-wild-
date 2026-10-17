#pragma once
// MIT-style test fixture: synthetic NSO0 file containing only two AArch64 instructions:
// add w0,w0,w1; ret. No Nintendo code or assets, and no externally supplied executable bytes.
#include "nso_memory.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace synthetic {
inline constexpr std::array<std::uint8_t,8> kAllowedText = {
    0x00,0x00,0x01,0x0b, // 0x0B010000: add w0, w0, w1
    0xc0,0x03,0x5f,0xd6  // 0xD65F03C0: ret
};
inline void write32(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value) {
    for(int i=0;i<4;i++) bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i));
}
inline std::vector<std::uint8_t> makeNso() {
    std::vector<std::uint8_t> bytes(0x140,0);
    bytes[0]='N';bytes[1]='S';bytes[2]='O';bytes[3]='0';
    write32(bytes,0x0c,1u|8u|16u|32u); // LZ4 .text and SHA-256 for all three sections
    write32(bytes,0x10,0x100);write32(bytes,0x14,0);write32(bytes,0x18,8);
    write32(bytes,0x20,0x120);write32(bytes,0x24,0x100);write32(bytes,0x28,4);
    write32(bytes,0x30,0x130);write32(bytes,0x34,0x200);write32(bytes,0x38,4);
    write32(bytes,0x3c,8); // BSS
    write32(bytes,0x60,9);write32(bytes,0x64,4);write32(bytes,0x68,4);
    bytes[0x100]=0x80; // LZ4: 8-byte literal without a match
    std::memcpy(bytes.data()+0x101,kAllowedText.data(),kAllowedText.size());
    constexpr std::array<std::uint8_t,4> rodata={'T','E','S','T'};
    constexpr std::array<std::uint8_t,4> data={'D','A','T','A'};
    std::memcpy(bytes.data()+0x120,rodata.data(),rodata.size());
    std::memcpy(bytes.data()+0x130,data.data(),data.size());
    const auto a=digest256::hash(kAllowedText.data(),kAllowedText.size());
    const auto b=digest256::hash(rodata.data(),rodata.size());
    const auto c=digest256::hash(data.data(),data.size());
    std::memcpy(bytes.data()+0xa0,a.data(),a.size());
    std::memcpy(bytes.data()+0xc0,b.data(),b.size());
    std::memcpy(bytes.data()+0xe0,c.data(),c.size());
    return bytes;
}
inline bool allowed(const nsomemory::Image& image) {
    if(!image.ok || image.hashChecks!=3 ||
       image.sectionLengths[0]!=kAllowedText.size() ||
       image.sectionLengths[1]!=4 || image.sectionLengths[2]!=4 ||
       image.bssLength!=8 || image.sectionOffsets[0]!=0) return false;
    if(image.bytes.size() < kAllowedText.size())return false;
    return std::memcmp(image.bytes.data(),kAllowedText.data(),kAllowedText.size())==0;
}
} // namespace synthetic
