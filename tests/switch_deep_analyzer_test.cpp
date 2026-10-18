// Synthetic, original test containers; no commercial code or keys.
#include "../app/src/main/cpp/switch_deep_analyzer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

using Bytes=std::vector<unsigned char>;

void w32(Bytes& b,size_t p,uint32_t v) {
    assert(p+4<=b.size());
    for(int i=0;i<4;i++)b[p+i]=static_cast<unsigned char>(v>>(i*8));
}
void w64(Bytes& b,size_t p,uint64_t v) {
    w32(b,p,static_cast<uint32_t>(v));
    w32(b,p+4,static_cast<uint32_t>(v>>32));
}
void replace(Bytes& b,size_t at,const Bytes& src) {
    if(b.size()<at+src.size())b.resize(at+src.size(),0);
    std::memcpy(b.data()+at,src.data(),src.size());
}
Bytes pfs(const std::vector<std::pair<std::string,Bytes>>& entries,bool hfs=false) {
    size_t strsize=0,payloadsize=0;
    for(const auto& e:entries){strsize+=e.first.size()+1;payloadsize+=e.second.size();}
    const size_t stride=hfs?64:24;
    const size_t dataStart=16+entries.size()*stride+strsize;
    Bytes b(dataStart+payloadsize,0);
    std::memcpy(b.data(),hfs?"HFS0":"PFS0",4);
    w32(b,4,static_cast<uint32_t>(entries.size()));
    w32(b,8,static_cast<uint32_t>(strsize));
    size_t fileOff=0,nameOff=0,contentOff=dataStart;
    for(size_t i=0;i<entries.size();i++){
        const auto& [name,data]=entries[i];
        w64(b,16+i*stride,fileOff);
        w64(b,16+i*stride+8,data.size());
        w32(b,16+i*stride+16,static_cast<uint32_t>(nameOff));
        std::memcpy(b.data()+16+entries.size()*stride+nameOff,
                    name.c_str(),name.size()+1);
        replace(b,contentOff,data);
        contentOff+=data.size();
        fileOff+=data.size();
        nameOff+=name.size()+1;
    }
    return b;
}
std::string check(const Bytes& bytes) {
    char name[]="/tmp/botw-deep-XXXXXX";
    const int fd=mkstemp(name);
    assert(fd>=0);
    unlink(name);
    size_t at=0;
    while(at<bytes.size()){
        const auto w=write(fd,bytes.data()+at,bytes.size()-at);
        assert(w>0);
        at+=size_t(w);
    }
    const std::string result=switchdeep::inspect(fd);
    close(fd);
    return result;
}
bool contains(const std::string& s,const std::string& w) {
    return s.find(w)!=std::string::npos;
}
Bytes nca(bool plain) {
    Bytes b(0x400,0x6a);
    if(plain) {
        std::memcpy(b.data()+0x200,"NCA3",4);
        b[0x205]=0; // Program
        w64(b,0x208,b.size());
        w64(b,0x210,0x01007EF00011E000ull);
    }
    return b;
}
int main() {
    {
        const auto image=pfs({{"one.nca",nca(false)},
                              {"two.cnmt.nca",nca(true)},
                              {"test.tik",Bytes(10,0)},
                              {"main.nso",Bytes(0x100,0)}});
        const auto report=check(image);
        assert(contains(report,"Formato: NSP"));
        assert(contains(report,"NCA: 2"));
        assert(contains(report,"sin cabecera legible 1"));
        assert(contains(report,"PROGRAM (candidato a codigo)"));
        assert(contains(report,"Tickets: 1"));
        assert(contains(report,"NSO no validado"));
        assert(!contains(report,"PRIVATE_KEY"));
        std::cout<<"PASS: NSP breakdown, encrypted NCA, visible Program and direct NSO\n";
    }
    {
        const auto inside=pfs({{"content.nca",nca(false)},
                               {"metadata.cnmt.nca",nca(true)}},true);
        const auto root=pfs({{"secure",inside},{"normal",pfs({},true)}},true);
        Bytes image(0x1000,0);
        std::memcpy(image.data()+0x100,"HEAD",4);
        w64(image,0x130,0x1000);
        w64(image,0x138,0x200); // Padded partition header advertised
        replace(image,0x1000,root);
        const auto report=check(image);
        assert(contains(report,"Formato: XCI"));
        assert(contains(report,"Particiones raiz: 2"));
        assert(contains(report,"Contenido secure: 2 archivos"));
        assert(contains(report,"NCA: 2"));
        std::cout<<"PASS: XCI root HFS0 and nested secure partition breakdown\n";
    }
    {
        Bytes image(0x1100,0);
        std::memcpy(image.data()+0x100,"HEAD",4);
        w64(image,0x130,UINT64_MAX-20);
        w64(image,0x138,0x200);
        assert(contains(check(image),"RECHAZADO"));
        std::cout<<"PASS: XCI offset overflow safely rejected\n";
    }
    {
        auto image=pfs({{"wrong.nca",Bytes(2,0)}});
        // Corrupt PFS0 offset to exceed the file.
        w64(image,16,0xFFFFFFFFFFFFFFFFull);
        assert(contains(check(image),"RECHAZADO"));
        std::cout<<"PASS: out-of-range PFS0 offsets safely rejected\n";
    }
    {
        Bytes image(0x260,0);
        std::memcpy(image.data(),"NSO0",4);
        w32(image,0x10,0x100);w32(image,0x14,0);w32(image,0x18,0x80);
        w32(image,0x20,0x180);w32(image,0x24,0x100);w32(image,0x28,0x80);
        w32(image,0x30,0x200);w32(image,0x34,0x200);w32(image,0x38,0x20);
        w32(image,0x3C,0x20);
        w32(image,0x60,0x80);w32(image,0x64,0x80);w32(image,0x68,0x20);
        const auto report=check(image);
        assert(contains(report,"Formato: NSO0"));
        assert(contains(report,".text=128 bytes"));
        assert(contains(report,"MOD0"));
        assert(contains(report,"candidato"));
        std::cout<<"PASS: standalone NSO section map and bounded MOD0 attempt\n";
    }
    {
        Bytes image(0x300,0);
        std::memcpy(image.data()+0x10,"NRO0",4);
        w32(image,0x18,0x300);
        w32(image,0x20,0);w32(image,0x24,0x100);
        w32(image,0x28,0x100);w32(image,0x2C,0x100);
        w32(image,0x30,0x200);w32(image,0x34,0x100);
        const auto report=check(image);
        assert(contains(report,"Formato: NRO0"));
        assert(contains(report,"HOMEbrew"));
        std::cout<<"PASS: standalone NRO code sections reported\n";
    }
    {
        const auto report=check(Bytes(0x500,0));
        assert(contains(report,"RECHAZADO"));
        std::cout<<"PASS: unknown format rejected without pretending it is Zelda\n";
    }
    std::cout<<"All deep analyzer synthetic test cases passed.\n";
}
