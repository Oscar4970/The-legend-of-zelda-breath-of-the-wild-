#include "../app/src/main/cpp/nso_format.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

using Header=std::array<unsigned char,nso::kHeaderSize>;
static void set32(Header& b, std::size_t at, std::uint32_t v) {
    for(int i=0;i<4;i++) b[at+i]=static_cast<unsigned char>((v>>(8*i))&0xff);
}
static Header sample() {
    Header b{};
    b[0]='N';b[1]='S';b[2]='O';b[3]='0';
    set32(b,0x04,0);
    set32(b,0x0c,1); // compressed text, uncompressed rodata and data
    set32(b,0x10,0x100);set32(b,0x14,0);set32(b,0x18,0x200);
    set32(b,0x20,0x180);set32(b,0x24,0x200);set32(b,0x28,0x100);
    set32(b,0x30,0x280);set32(b,0x34,0x300);set32(b,0x38,0x100);
    set32(b,0x3c,0x80);
    set32(b,0x60,0x80);set32(b,0x64,0x100);set32(b,0x68,0x100);
    b[0x40]=0xde;b[0x41]=0xad;
    return b;
}
int main(){
    auto b=sample();
    auto info=nso::inspect(b.data(),b.size(),0x380);
    assert(info.valid);
    assert(info.sections[0].compressed && !info.sections[1].compressed);
    assert(info.sections[0].memSize==0x200);
    assert(info.sections[0].storedSize==0x80);
    assert(info.bssSize==0x80 && info.moduleId.substr(0,4)=="dead");
    assert(nso::report(info).find("NO ejecuta")!=std::string::npos);
    std::cout<<"PASS: valid compressed NSO header\n";
    assert(!nso::inspect(b.data(),0x80,0x380).valid);
    std::cout<<"PASS: truncated header rejected\n";
    auto bad=b; bad[0]='X';
    assert(!nso::inspect(bad.data(),bad.size(),0x380).valid);
    std::cout<<"PASS: invalid signature rejected\n";
    assert(!nso::inspect(b.data(),b.size(),0x150).valid);
    std::cout<<"PASS: missing section data rejected\n";
    bad=b; set32(bad,0x20,0x170);
    assert(!nso::inspect(bad.data(),bad.size(),0x380).valid);
    std::cout<<"PASS: overlapping sections rejected\n";
    bad=b; set32(bad,0x18,0xffffffffu);
    assert(!nso::inspect(bad.data(),bad.size(),0x380).valid);
    std::cout<<"PASS: decompression bomb bound rejected\n";
    bad=b; set32(bad,0x60,0);
    assert(!nso::inspect(bad.data(),bad.size(),0x380).valid);
    std::cout<<"PASS: zero stored size rejected\n";
    bad=b; set32(bad,0x68,0x90);
    assert(!nso::inspect(bad.data(),bad.size(),0x380).valid);
    std::cout<<"PASS: inconsistent uncompressed size rejected\n";
    bad=b; set32(bad,0x30,0xffffffffu);
    assert(!nso::inspect(bad.data(),bad.size(),0x380).valid);
    std::cout<<"PASS: file-size arithmetic overflow rejected\n";
    assert(!nso::inspect(b.data(),b.size(),nso::kMaxFileSize+1).valid);
    std::cout<<"PASS: oversized files rejected\n";
    bad=b;set32(bad,0x20,0x380);set32(bad,0x28,0);set32(bad,0x64,0);
    assert(nso::inspect(bad.data(),bad.size(),0x380).valid);
    std::cout<<"PASS: valid empty rodata accepted\n";
    std::cout<<"All 11 tests passed.\n";
}
