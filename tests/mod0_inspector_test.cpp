#include "../app/src/main/cpp/mod0_inspector.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
constexpr std::size_t kText=0x100, kRo=0x180, kData=0x780;
constexpr std::size_t kFileSize=0x7c0;
void put32(std::vector<std::uint8_t>& v,std::size_t off,std::uint32_t x) {
    assert(off+4<=v.size());
    for(int i=0;i<4;i++)v[off+i]=static_cast<std::uint8_t>(x>>(8*i));
}
void put64(std::vector<std::uint8_t>& v,std::size_t off,std::uint64_t x) {
    assert(off+8<=v.size());
    for(int i=0;i<8;i++)v[off+i]=static_cast<std::uint8_t>(x>>(8*i));
}
void hashes(std::vector<std::uint8_t>& v){
    const auto a=digest256::hash(v.data()+kText,0x80);
    const auto b=digest256::hash(v.data()+kRo,0x600);
    const auto c=digest256::hash(v.data()+kData,0x40);
    std::memcpy(v.data()+0xa0,a.data(),32);
    std::memcpy(v.data()+0xc0,b.data(),32);
    std::memcpy(v.data()+0xe0,c.data(),32);
}
void dyn(std::vector<std::uint8_t>& file,std::size_t i,std::uint64_t tag,std::uint64_t value) {
    put64(file,kRo+i*16,tag);
    put64(file,kRo+i*16+8,value);
}
std::vector<std::uint8_t> fixture(){
    std::vector<std::uint8_t> f(kFileSize,0);
    std::memcpy(f.data(),"NSO0",4);
    put32(f,0x0c,8|16|32); // all hashes, no compression
    put32(f,0x10,kText);put32(f,0x14,0);put32(f,0x18,0x80);
    put32(f,0x20,kRo);put32(f,0x24,0x1000);put32(f,0x28,0x600);
    put32(f,0x30,kData);put32(f,0x34,0x2000);put32(f,0x38,0x40);
    put32(f,0x3c,0x20); // BSS 32 bytes
    put32(f,0x60,0x80);put32(f,0x64,0x600);put32(f,0x68,0x40);

    // ModuleHeaderLocation at start of the .text section, offset +4 -> MOD0.
    put32(f,kText+4,0x20);
    std::memcpy(f.data()+kText+0x20,"MOD0",4);
    put32(f,kText+0x24,0x1000-0x20); // .dynamic - mod header
    put32(f,kText+0x28,0x2040-0x20); // BSS start - mod header
    put32(f,kText+0x2c,0x2060-0x20); // BSS end - mod header
    put32(f,kText+0x38,0x2040-0x20); // runtime module offset

    const char strings[]="libDemo.nso\0demoImport\0";
    static_assert(sizeof(strings)==24, "includes final implicit NUL");
    std::memcpy(f.data()+kRo+0x200,strings,sizeof(strings));
    dyn(f,0,5,0x1200); // DT_STRTAB
    dyn(f,1,10,sizeof(strings)); // DT_STRSZ
    dyn(f,2,1,0); // DT_NEEDED libDemo.nso
    dyn(f,3,6,0x1280); // DT_SYMTAB
    dyn(f,4,11,24); // DT_SYMENT
    dyn(f,5,7,0x1300); // DT_RELA
    dyn(f,6,8,48); // DT_RELASZ
    dyn(f,7,9,24); // DT_RELAENT
    dyn(f,8,0,0); // DT_NULL

    // ELF64 Sym[1]: st_name offset 12 into DT_STRTAB
    put32(f,kRo+0x280+24,12);

    // RELATIVE relocation into .data
    put64(f,kRo+0x300,0x2000);
    put64(f,kRo+0x308,1027);
    put64(f,kRo+0x310,0);

    // GLOB_DAT relocation referencing symbol 1 into .data
    put64(f,kRo+0x318,0x2008);
    put64(f,kRo+0x320,(std::uint64_t(1)<<32)|1025);
    put64(f,kRo+0x328,0);
    hashes(f);
    return f;
}
modinspect::Result read(std::vector<std::uint8_t>& f){
    const auto memory=nsomemory::reconstruct(f.data(),f.size());
    assert(memory.ok);
    return modinspect::parse(memory);
}
void pass(const char* msg){std::cout<<"PASS: "<<msg<<"\n";}
}

int main(){
    auto file=fixture();
    auto result=read(file);
    if(!result.ok)std::cerr<<"Fixture rejected: "<<result.error<<"\n";
    assert(result.ok);
    assert(result.modVa==0x20 && result.dynamicVa==0x1000);
    assert(result.dynamicEntries==9 && result.relaEntries==2);
    assert(result.needed.size()==1 && result.needed[0]=="libDemo.nso");
    assert(result.importsFound==1 && result.importNames.size()==1 &&
           result.importNames[0]=="demoImport");
    assert(result.relative==1 && result.globDat==1);
    pass("MOD0 parsed, DT_NEEDED dependency and ELF64 AArch64 relocations inventoried");

    auto aligned=fixture();
    // MOD0 BSS may start inside initialized .data and end before the mapping end.
    put32(aligned,kText+0x28,0x2030-0x20);
    put32(aligned,kText+0x2c,0x2058-0x20);
    hashes(aligned);
    assert(read(aligned).ok);
    pass("MOD0 BSS interval within data+BSS mapping accepted");
    auto bad=fixture();
    bad[kText+0x20]='N';hashes(bad);
    assert(!read(bad).ok);pass("invalid MOD0 signature rejected");
    bad=fixture();
    put32(bad,kText+4,0x7fffffff);hashes(bad);
    assert(!read(bad).ok);pass("invalid ModuleHeaderLocation rejected");
    bad=fixture();
    put32(bad,kText+0x24,0x7000);hashes(bad);
    assert(!read(bad).ok);pass("dynamic outside NSO sections rejected");
    bad=fixture();
    put32(bad,kText+0x2c,0x2070-0x20);hashes(bad);
    assert(!read(bad).ok);pass("invalid MOD0 BSS bounds rejected");
    bad=fixture();
    dyn(bad,8,5,0x1200);hashes(bad);
    assert(!read(bad).ok);pass("duplicate dynamic tag rejected");
    bad=fixture();
    dyn(bad,2,1,9999);hashes(bad);
    assert(!read(bad).ok);pass("invalid DT_NEEDED string offset rejected");
    bad=fixture();
    put64(bad,kRo+0x300,0x3000);hashes(bad);
    assert(!read(bad).ok);pass("relocation target outside mapped memory rejected");
    bad=fixture();
    put64(bad,kRo+0x320,(std::uint64_t(65000)<<32)|1025);hashes(bad);
    assert(!read(bad).ok);pass("out-of-range ELF64 symbol index rejected");
    bad=fixture();
    dyn(bad,7,9,8);hashes(bad);
    assert(!read(bad).ok);pass("invalid ELF64 RELAENT size rejected");
    bad=fixture();
    put64(bad,kRo+0x300,0x2048);hashes(bad);
    auto bss=read(bad);
    assert(bss.ok);pass("relocation into zero-initialized BSS accepted for inventory");
    bad=fixture();
    bad[kRo+0x200]='X'; // don't update SHA, must fail at NSO reconstruction.
    auto memory=nsomemory::reconstruct(bad.data(),bad.size());
    assert(!memory.ok);pass("corrupted NSO input blocked by SHA-256 before MOD0");
    std::cout<<"All 13 MOD0 parser tests passed.\n";
}
