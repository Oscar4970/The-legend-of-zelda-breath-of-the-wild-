#include "../app/src/main/cpp/nso_memory.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
static void wr(std::vector<std::uint8_t>& b,std::size_t off,std::uint32_t x){
    for(int j=0;j<4;j++)b[off+j]=static_cast<std::uint8_t>((x>>(j*8))&255);
}
static std::vector<std::uint8_t> sample(){
    std::vector<std::uint8_t> b(0x130);
    b[0]='N';b[1]='S';b[2]='O';b[3]='0';
    wr(b,0xc,1);
    // compressed "ABABABAB": token 0x22, literals AB, match offset 2, length 6.
    wr(b,0x10,0x100);wr(b,0x14,0);wr(b,0x18,8);wr(b,0x60,5);
    wr(b,0x20,0x110);wr(b,0x24,0x100);wr(b,0x28,4);wr(b,0x64,4);
    wr(b,0x30,0x120);wr(b,0x34,0x200);wr(b,0x38,4);wr(b,0x68,4);
    wr(b,0x3c,8);
    b[0x100]=0x22;b[0x101]='A';b[0x102]='B';b[0x103]=2;b[0x104]=0;
    std::memcpy(b.data()+0x110,"READ",4);
    std::memcpy(b.data()+0x120,"DATA",4);
    return b;
}
int main(){
    auto f=sample();
    auto r=nsomemory::reconstruct(f.data(),f.size());
    assert(r.ok); assert(r.bytes.size()==0x20c);
    assert(std::memcmp(r.bytes.data(),"ABABABAB",8)==0);
    assert(std::memcmp(r.bytes.data()+0x100,"READ",4)==0);
    assert(std::memcmp(r.bytes.data()+0x200,"DATA",4)==0);
    for(int i=0;i<8;i++) assert(r.bytes[0x204+i]==0);
    std::cout<<"PASS: LZ4 overlap, 3 mapped sections, zeroed BSS\n";
    auto bad=f;bad[0x103]=0;bad[0x104]=0;
    assert(!nsomemory::reconstruct(bad.data(),bad.size()).ok);
    std::cout<<"PASS: invalid LZ4 zero offset rejected\n";
    bad=f;bad[0x100]=0xf0;
    assert(!nsomemory::reconstruct(bad.data(),bad.size()).ok);
    std::cout<<"PASS: truncated length extension rejected\n";
    bad=f;wr(bad,0x24,3);
    assert(!nsomemory::reconstruct(bad.data(),bad.size()).ok);
    std::cout<<"PASS: overlapped memory regions rejected\n";
    bad=f;wr(bad,0x18,9);
    assert(!nsomemory::reconstruct(bad.data(),bad.size()).ok);
    std::cout<<"PASS: expected-size mismatch rejected\n";
    bad=f;wr(bad,0x3c,16*1024*1024);
    assert(!nsomemory::reconstruct(bad.data(),bad.size()).ok);
    std::cout<<"PASS: huge BSS rejected\n";
    bad=f;wr(bad,0x34,0x40000000);
    assert(!nsomemory::reconstruct(bad.data(),bad.size()).ok);
    std::cout<<"PASS: excessive sparse address range rejected\n";
    assert(!nsomemory::lz4Block(reinterpret_cast<const std::uint8_t*>("\x10Q"),2,
                                 reinterpret_cast<std::uint8_t*>(bad.data()),0));
    std::cout<<"PASS: output overrun rejected\n";

    const auto abc=digest256::hash(reinterpret_cast<const std::uint8_t*>("abc"),3);
    assert(digest256::asHex(abc)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const auto empty=digest256::hash(reinterpret_cast<const std::uint8_t*>(""),0);
    assert(digest256::asHex(empty)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    std::cout<<"PASS: FIPS SHA-256 known vectors\n";
    auto hashed=f;
    wr(hashed,0xc,1|8|16|32);
    const std::array<std::array<std::uint8_t,8>,3> sections={{{{'A','B','A','B','A','B','A','B'}},
                                                             {{'R','E','A','D',0,0,0,0}},
                                                             {{'D','A','T','A',0,0,0,0}}}};
    const std::size_t hashes[]={8,4,4};
    for(int i=0;i<3;i++){
        auto h=digest256::hash(sections[i].data(),hashes[i]);
        std::memcpy(hashed.data()+0xA0+i*32,h.data(),32);
    }
    const auto verified=nsomemory::reconstruct(hashed.data(),hashed.size());
    assert(verified.ok && verified.hashChecks==3);
    std::cout<<"PASS: all three hashes validated after decompression\n";
    hashed[0x110]^=1;
    const auto corrupted=nsomemory::reconstruct(hashed.data(),hashed.size());
    assert(!corrupted.ok && corrupted.error.find("SHA-256")!=std::string::npos);
    std::cout<<"PASS: corrupted section rejected by SHA-256\n";
    auto noHash=sample();
    wr(noHash,0xc,1|8);
    assert(!nsomemory::reconstruct(noHash.data(),noHash.size()).ok);
    std::cout<<"PASS: missing expected hash rejected\n";
    auto newer=sample();
    wr(newer,0xc,1|0x80);
    assert(!nsomemory::reconstruct(newer.data(),newer.size()).ok);
    std::cout<<"PASS: unsupported ZBIC explicitly rejected\n";
    std::cout<<"All 13 NSO memory tests passed.\n";
}
