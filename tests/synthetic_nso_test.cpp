#include "../app/src/main/cpp/synthetic_nso.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
    const auto file=synthetic::makeNso();
    const auto info=nso::inspect(file.data(),nso::kHeaderSize,file.size());
    assert(info.valid && (info.flags&0x39)==0x39);
    std::cout<<"PASS: built-in synthetic NSO header and flags\n";
    const auto image=nsomemory::reconstruct(file.data(),file.size());
    assert(image.ok && image.bytes.size()==0x20c);
    assert(synthetic::allowed(image));
    assert(image.hashChecks==3);
    assert(image.bytes[0]==0x00 && image.bytes[1]==0x00 && image.bytes[2]==0x01
           && image.bytes[3]==0x0b);
    std::cout<<"PASS: LZ4 decompression, SHA-256, strict ARM64 allowlist\n";
    assert(std::memcmp(image.bytes.data()+image.sectionOffsets[1],"TEST",4)==0);
    assert(std::memcmp(image.bytes.data()+image.sectionOffsets[2],"DATA",4)==0);
    for(int i=0;i<8;i++) assert(image.bytes[image.bssOffset+i]==0);
    std::cout<<"PASS: mapped rodata, data, and zeroed BSS\n";
    auto changed=file;
    changed[0x103]^=1;
    assert(!nsomemory::reconstruct(changed.data(),changed.size()).ok);
    std::cout<<"PASS: corrupted instruction fails SHA-256\n";
    changed=file;
    // Forge hash for modified code: SHA alone is not an authenticity signature;
    // strict allowlisting must independently deny this alternate function.
    changed[0x103]^=1;
    const auto alternate=digest256::hash(changed.data()+0x101,8);
    std::memcpy(changed.data()+0xa0,alternate.data(),32);
    const auto altered=nsomemory::reconstruct(changed.data(),changed.size());
    assert(altered.ok && !synthetic::allowed(altered));
    std::cout<<"PASS: forged valid checksum still rejected by instruction allowlist\n";
    std::cout<<"All 5 synthetic execution prerequisite tests passed. Actual ARM64 execution requires Android device.\n";
}
