#include "../app/src/main/cpp/nro_runtime_audit.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
namespace {
void put32(std::vector<std::uint8_t>& data,std::size_t at,std::uint32_t value){
    for(int i=0;i<4;i++)data[at+i]=static_cast<std::uint8_t>(value>>(i*8));
}
std::vector<std::uint8_t> fixture(){
    std::vector<std::uint8_t> data(0x240,0);
    std::memcpy(data.data()+0x10,"NRO0",4);
    put32(data,0x18,0x240);
    put32(data,0x20,0);put32(data,0x24,0x100);
    put32(data,0x28,0x100);put32(data,0x2c,0x100);
    put32(data,0x30,0x200);put32(data,0x34,0x40);
    put32(data,0x38,0x20);
    put32(data,0,0x14000020); // B +0x80
    put32(data,0x80,0xd503201f); // nop
    put32(data,0x84,0xd4000021); // svc #1
    put32(data,0x88,0xd40000c1); // svc #6
    put32(data,0x8c,0xd4200000); // brk #0
    return data;
}
void pass(const char* msg){std::cout<<"PASS: "<<msg<<"\n";}
}
int main(){
    auto file=fixture();
    auto audit=runtimeaudit::examine(file.data(),file.size());
    assert(audit.nroValid && audit.entryBranch && audit.entryTargetInsideText);
    assert(audit.entryTarget==0x80);
    pass("NRO0 entry branch agrees with Switch Homebrew ABI");
    assert(audit.svcSites==2 && audit.brkSites==1);
    assert(audit.svcIds.size()==2 &&
           audit.svcIds[0]==1 && audit.svcIds[1]==6);
    pass("AArch64 SVC and BRK patterns recognized in code section");
    assert(runtimeaudit::display(audit).find("NO se ejecuto")!=std::string::npos);
    pass("report explicitly states code was not executed");
    auto invalid=file;put32(invalid,0,0xd503201f);
    audit=runtimeaudit::examine(invalid.data(),invalid.size());
    assert(audit.nroValid && !audit.entryBranch && !audit.entryTargetInsideText);
    pass("missing NRO entry branch flagged");
    invalid=file;put32(invalid,0,0x17fffffc); // B -0x10
    audit=runtimeaudit::examine(invalid.data(),invalid.size());
    assert(audit.entryBranch && !audit.entryTargetInsideText);
    pass("entry branch outside .text rejected");
    invalid=file;invalid[0x10]='Z';
    assert(!runtimeaudit::examine(invalid.data(),invalid.size()).nroValid);
    pass("invalid NRO rejected before instruction scan");
    using runtimeaudit::ConfigEntry;
    const ConfigEntry entries[3]={
        {1,0,{0x99,0}}, // Test dummy handle, NEVER provided to actual code
        {7,0,{0,0}},
        {0,0,{0,0}}
    };
    assert(runtimeaudit::validateConfig(entries,3).ok);
    pass("Homebrew ABI 24-byte LoaderConfig layout validated");
    ConfigEntry malformed[3]={entries[0],entries[1],entries[2]};
    malformed[0].values[0]=0;
    assert(!runtimeaudit::validateConfig(malformed,3).ok);
    pass("missing main thread handle rejected");
    malformed[0]=entries[0];malformed[1].key=999;malformed[1].flags=1;
    assert(!runtimeaudit::validateConfig(malformed,3).ok);
    pass("unknown mandatory loader key rejected");
    malformed[1]=entries[1];malformed[2].key=1;
    assert(!runtimeaudit::validateConfig(malformed,3).ok);
    pass("missing EndOfList rejected");
    malformed[2]=entries[2];malformed[1].values[0]=12;
    assert(!runtimeaudit::validateConfig(malformed,3).ok);
    pass("invalid AppletType rejected");
    std::cout<<"All 11 runtime preflight tests passed.\n";
}
