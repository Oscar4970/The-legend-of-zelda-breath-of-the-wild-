#include "../app/src/main/cpp/nro_inspector.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
namespace {
void wr32(std::vector<std::uint8_t>& v,std::size_t offset,std::uint32_t x){
    for(int i=0;i<4;i++)v[offset+i]=static_cast<std::uint8_t>(x>>(i*8));
}
void wr64(std::vector<std::uint8_t>& v,std::size_t offset,std::uint64_t x){
    for(int i=0;i<8;i++)v[offset+i]=static_cast<std::uint8_t>(x>>(i*8));
}
std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> v(0x280,0);
    std::memcpy(v.data()+0x10,"NRO0",4);
    wr32(v,0x14,0);wr32(v,0x18,0x240);wr32(v,0x1c,0);
    wr32(v,0x20,0);wr32(v,0x24,0x100);
    wr32(v,0x28,0x100);wr32(v,0x2c,0x100);
    wr32(v,0x30,0x200);wr32(v,0x34,0x40);
    wr32(v,0x38,0x20);
    v[0x40]=0xab;v[0x41]=0xcd;
    // ModuleHeaderLocation at file virtual base+4; MOD0 at 0x80.
    wr32(v,4,0x80);
    std::memcpy(v.data()+0x80,"MOD0",4);
    wr32(v,0x84,0x80); // .dynamic 0x100
    wr32(v,0x88,0x1c0); // BSS start 0x240
    wr32(v,0x8c,0x1e0); // BSS end 0x260
    wr32(v,0x98,0x1c0); // ModuleOffset (unused by parser)
    wr64(v,0x100,5);wr64(v,0x108,0x180); // DT_STRTAB
    wr64(v,0x110,10);wr64(v,0x118,12); // DT_STRSZ
    wr64(v,0x120,1);wr64(v,0x128,0); // DT_NEEDED offset=0
    wr64(v,0x130,0);wr64(v,0x138,0); // DT_NULL
    std::memcpy(v.data()+0x180,"libDemo.nso",12);
    // Homebrew ASET appended AFTER the declared NRO code image.
    std::memcpy(v.data()+0x240,"ASET",4);
    wr64(v,0x248,0x38);wr64(v,0x250,4);
    std::memcpy(v.data()+0x278,"ICON",4);
    return v;
}
void pass(const char* value){std::cout<<"PASS: "<<value<<"\n";}
}
int main(){
    auto file=fixture();
    auto result=nroinspect::inspect(file.data(),file.size());
    if(!result.ok)std::cerr<<result.error<<"\n";
    assert(result.ok && result.codeSize==0x240 && result.bssSize==0x20);
    assert(result.lengths[0]==0x100 && result.lengths[1]==0x100 && result.lengths[2]==0x40);
    pass("NRO0 valid sections and BSS inspected");
    assert(result.moduleId.substr(0,4)=="abcd");
    assert(result.assetsPresent && result.assets[0].present && result.assets[0].size==4);
    pass("module ID and homebrew ASET icon read");
    assert(result.mod.ok && result.mod.needed.size()==1 && result.mod.needed[0]=="libDemo.nso");
    pass("NRO contained MOD0 and DT_NEEDED dependency read");
    auto bad=fixture();bad[0x10]='X';
    assert(!nroinspect::inspect(bad.data(),bad.size()).ok);
    pass("invalid NRO signature rejected");
    bad=fixture();bad.resize(0x230);
    assert(!nroinspect::inspect(bad.data(),bad.size()).ok);
    pass("truncated NRO code rejected");
    bad=fixture();wr32(bad,0x28,0xf0);
    assert(!nroinspect::inspect(bad.data(),bad.size()).ok);
    pass("overlapping .text/.rodata rejected");
    bad=fixture();wr32(bad,0x34,0x100);
    assert(!nroinspect::inspect(bad.data(),bad.size()).ok);
    pass("out-of-range .data rejected");
    bad=fixture();wr32(bad,0x38,0x400001);
    assert(!nroinspect::inspect(bad.data(),bad.size()).ok);
    pass("oversized BSS rejected");
    bad=fixture();wr64(bad,0x248,0xffffffffffffffffULL);
    assert(!nroinspect::inspect(bad.data(),bad.size()).ok);
    pass("invalid ASET offset rejected");
    bad=fixture();wr32(bad,0x1c,2);
    assert(!nroinspect::inspect(bad.data(),bad.size()).ok);
    pass("unsupported compressed NRO rejected");
    bad=fixture();bad.resize(0x240);
    auto withoutAssets=nroinspect::inspect(bad.data(),bad.size());
    assert(withoutAssets.ok && !withoutAssets.assetsPresent);
    pass("NRO without optional ASET accepted");
    bad=fixture();bad[0x80]='X';
    auto brokenMod=nroinspect::inspect(bad.data(),bad.size());
    assert(brokenMod.ok && !brokenMod.mod.ok);
    pass("NRO identification retained when MOD0 invalid");
    bad=fixture();wr32(bad,0x24,0x40);
    assert(!nroinspect::inspect(bad.data(),bad.size()).ok);
    pass("truncated NRO text/header rejected");
    std::cout<<"All 13 NRO inspection tests passed.\n";
}
