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
    std::cout<<"All 8 NSO memory tests passed.\n";
}
