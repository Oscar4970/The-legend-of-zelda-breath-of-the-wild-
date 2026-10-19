// Synthetic read-only NSP -> NCA ciphertext samples, never game data.
#include "../app/src/main/cpp/switch_nca_bridge.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

using Bytes=std::vector<unsigned char>;
static void put32(Bytes& b,size_t at,uint32_t v) {
    for(unsigned i=0;i<4;i++) b.at(at+i)=static_cast<unsigned char>((v>>(i*8))&255);
}
static void put64(Bytes& b,size_t at,uint64_t v) {
    for(unsigned i=0;i<8;i++) b.at(at+i)=static_cast<unsigned char>((v>>(i*8))&255);
}
static Bytes makeNsp(bool corrupt=false,bool shortNca=false) {
    const std::string name1="program.nca",name2="info.tik";
    const size_t names=name1.size()+1+name2.size()+1;
    const size_t first=16+24*2+names;
    const size_t length=shortNca?16:0xC00;
    Bytes b(first+length+8,0);
    std::memcpy(b.data(),"PFS0",4);
    put32(b,4,2);
    put32(b,8,static_cast<uint32_t>(names));
    put64(b,16,corrupt?UINT64_MAX:0);
    put64(b,24,length);
    put32(b,32,0);
    put64(b,40,length);
    put64(b,48,8);
    put32(b,56,static_cast<uint32_t>(name1.size()+1));
    std::memcpy(b.data()+64,name1.c_str(),name1.size()+1);
    std::memcpy(b.data()+64+name1.size()+1,name2.c_str(),name2.size()+1);
    for(size_t i=0;i<length;i++) b[first+i]=static_cast<unsigned char>(i%256);
    return b;
}
static std::vector<ncabridge::Header> samples(const Bytes& bytes) {
    char name[]="/tmp/botw-nca-XXXXXX";
    int fd=mkstemp(name);
    assert(fd>=0);
    unlink(name);
    size_t p=0;
    while(p<bytes.size()) {
        ssize_t n=write(fd,bytes.data()+p,bytes.size()-p);
        assert(n>0);
        p+=static_cast<size_t>(n);
    }
    try {
        auto out=ncabridge::sampleHeaders(fd);
        close(fd);
        return out;
    } catch(...) {
        close(fd);
        throw;
    }
}
static void mustFail(const Bytes& b) {
    bool rejected=false;
    try { (void)samples(b); } catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
int main() {
    auto data=samples(makeNsp());
    assert(data.size()==1);
    assert(data[0].size()==0xC00);
    for(size_t i=0;i<0xC00;i++) assert(data[0][i]==i%256);
    mustFail(makeNsp(true,false));
    mustFail(makeNsp(false,true));
    mustFail(Bytes(100,0));
    std::cout<<"PASS: bounded NSP PFS0 NCA header sampling; no keys or decrypted assets\n";
}
