// Synthetic sparse 14 GiB NSP: read pages without allocating/copying payload.
// Proves the virtual mount retains FD ownership after picker FD closes.
#include "../app/src/main/cpp/switch_nsp_vfs.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

static void w32(unsigned char* p,uint32_t n) {
    for(int k=0;k<4;k++) p[k]=static_cast<unsigned char>(n>>(8*k));
}
static void w64(unsigned char* p,uint64_t n) {
    for(int k=0;k<8;k++) p[k]=static_cast<unsigned char>(n>>(8*k));
}
static void put(int fd, const void* buf,size_t size,uint64_t offset) {
    assert(pwrite(fd,buf,size,static_cast<off_t>(offset))==static_cast<ssize_t>(size));
}
static int fixture(bool duplicate=false) {
    char path[]="/tmp/botw-vfs-XXXXXX";
    int fd=mkstemp(path);
    assert(fd>=0);
    unlink(path);
    // Test >4GiB offsets with sparse file; no 14 GiB allocation.
    static constexpr uint64_t SIZE=14ull*1024*1024*1024;
    assert(ftruncate(fd,static_cast<off_t>(SIZE))==0);
    const std::string first="game.nca";
    const std::string second=duplicate?"game.nca":"meta.cnmt.nca";
    unsigned char head[16+48+64]{};
    std::memcpy(head,"PFS0",4);
    w32(head+4,2);
    w32(head+8,static_cast<uint32_t>(first.size()+1+second.size()+1));
    const uint64_t dataStart=16+48+first.size()+1+second.size()+1;
    const uint64_t firstSize=SIZE-dataStart-0x400;
    w64(head+16,0);
    w64(head+24,firstSize);
    w32(head+32,0);
    w64(head+40,firstSize);
    w64(head+48,0x400);
    w32(head+56,static_cast<uint32_t>(first.size()+1));
    std::memcpy(head+64,first.c_str(),first.size()+1);
    std::memcpy(head+64+first.size()+1,second.c_str(),second.size()+1);
    put(fd,head,static_cast<size_t>(dataStart),0);
    const unsigned char start=0xA7, middle=0xB8, end=0xC9;
    put(fd,&start,1,dataStart);
    put(fd,&middle,1,dataStart+firstSize/2);
    put(fd,&end,1,dataStart+firstSize-1);
    return fd;
}
static void invalidRead(const nspvfs::Mount& mount, const char* name, uint64_t offset) {
    bool rejected=false;
    unsigned char byte=0;
    try { mount.read(name,offset,&byte,1); }
    catch(const std::out_of_range&) { rejected=true; }
    assert(rejected);
}
int main() {
    int fd=fixture();
    auto mount=std::make_unique<nspvfs::Mount>(fd);
    close(fd); // file picker/Android caller is free to close original handle
    assert(mount->count()==2 && mount->ncaCount()==2);
    assert(mount->exists("game.nca") && !mount->exists("../game.nca"));
    const uint64_t size=mount->sizeOf("game.nca");
    unsigned char sample[3]{};
    mount->read("game.nca",0,sample,1);
    mount->read("game.nca",size/2,sample+1,1);
    mount->read("game.nca",size-1,sample+2,1);
    assert(sample[0]==0xA7 && sample[1]==0xB8 && sample[2]==0xC9);
    assert(mount->selfTest().find("3 lecturas aleatorias")!=std::string::npos);
    invalidRead(*mount,"game.nca",size);
    invalidRead(*mount,"../game.nca",0);
    invalidRead(*mount,"not-real.nca",0);
    // Strict range and null-pointer checks
    bool rejected=false;
    try { mount->read("game.nca",0,nullptr,1); }
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    mount->read("game.nca",size,nullptr,0);
    int bad=fixture(true);
    rejected=false;
    try { nspvfs::Mount unsafe(bad); }
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    close(bad);
    std::cout<<"PASS: NSP runtime VFS persistent random-access >4GiB, bounds, duplicate names, no extraction\n";
}
