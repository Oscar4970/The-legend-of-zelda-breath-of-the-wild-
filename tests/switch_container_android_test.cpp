#include "../app/src/main/cpp/switch_container_inspector.h"
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
using namespace switchfile;
struct Temp {
    int fd;
    Temp() {
        char temp[]="/tmp/inspect-switch-XXXXXX";
        fd=mkstemp(temp); assert(fd>=0); unlink(temp);
    }
    ~Temp(){close(fd);}
    void write(const std::vector<unsigned char>& x) {
        assert(ftruncate(fd,0)==0);
        assert(pwrite(fd,x.data(),x.size(),0)==ssize_t(x.size()));
    }
};
void w32(std::vector<unsigned char>& x,size_t p,uint32_t v) {
    for(int i=0;i<4;i++)x[p+i]=unsigned char((v>>(8*i))&255);
}
void w64(std::vector<unsigned char>& x,size_t p,uint64_t v) {
    for(int i=0;i<8;i++)x[p+i]=unsigned char((v>>(8*i))&255);
}
int main(){
    Temp f;
    const char* file="owned-example.nca";
    size_t names=strlen(file)+1;
    std::vector<unsigned char> x(16+24+names+5,0);
    memcpy(x.data(),"PFS0",4); w32(x,4,1);w32(x,8,uint32_t(names));
    w64(x,16,0);w64(x,24,5);w32(x,32,0);
    memcpy(x.data()+40,file,names);
    f.write(x);
    assert(inspect(f.fd).find("NCA: 1")!=std::string::npos);
    assert(inspect(f.fd).find("NO se leyeron")!=std::string::npos);
    x[0]='N';f.write(x);assert(inspect(f.fd).find("NO VALIDADO")!=std::string::npos);
    x[0]='P';w64(x,24,90000000);f.write(x);
    assert(inspect(f.fd).find("NO VALIDADO")!=std::string::npos);
    x.assign(0x200,0);memcpy(x.data()+0x100,"HEAD",4);f.write(x);
    assert(inspect(f.fd).find("Formato: XCI")!=std::string::npos);
    x[0]='P';x[1]='F';x[2]='S';x[3]='0';w32(x,4,UINT32_MAX);f.write(x);
    assert(inspect(f.fd).find("NO VALIDADO")!=std::string::npos);
    std::cout<<"PASS: read-only NSP/XCI metadata, malformed and bounds rejection\n";
}
