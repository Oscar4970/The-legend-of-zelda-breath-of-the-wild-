// Integration check against an independently published open-source Switch NRO.
// This program only reads and prints metadata; it NEVER executes NRO instructions.
#include "../app/src/main/cpp/relr_sandbox.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int main(int argc,char** argv) {
    if(argc!=2) {
        std::cerr<<"Usage: nro_open_homebrew_test path/to/public_homebrew.nro\n";
        return 2;
    }
    std::ifstream stream(argv[1],std::ios::binary);
    if(!stream){std::cerr<<"Missing input NRO.\n";return 2;}
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(stream)),
                                    std::istreambuf_iterator<char>());
    const auto result=nroinspect::inspect(data.data(),data.size());
    if(!result.ok){
        std::cerr<<"Open-source homebrew NRO rejected: "<<result.error<<"\n";
        return 1;
    }
    const std::uint32_t headerOffset=nroinspect::u32(data.data()+4);
    if(headerOffset+0x1c<data.size() && headerOffset>=0x80) {
        const auto* hdr=data.data()+headerOffset;
        auto disp=[&](std::size_t off){
            const auto signedOffset=static_cast<std::int32_t>(nroinspect::u32(hdr+off));
            return static_cast<std::int64_t>(headerOffset)+signedOffset;
        };
        std::cout<<"MOD0 diagnostic: off="<<headerOffset
                 <<" sig="<<std::string(reinterpret_cast<const char*>(hdr),4)
                 <<" dynamic="<<disp(4)
                 <<" bssStart="<<disp(8)
                 <<" bssEnd="<<disp(12)
                 <<" dataEnd="<<(result.offsets[2]+result.lengths[2])
                 <<" bssLength="<<result.bssSize<<"\n";
    }
    std::cout<<"PASS: public independent open-source NRO0, "
             <<data.size()<<" source bytes\n"
             <<"Text="<<result.lengths[0]<<" Ro="<<result.lengths[1]
             <<" Data="<<result.lengths[2]<<" BSS="<<result.bssSize<<"\n"
             <<"ASET present="<<(result.assetsPresent?"yes":"no")<<"\n"
             <<"MOD0="<<(result.mod.ok?"parsed":"not parsed")
             <<(result.mod.ok?"":": "+result.mod.error)<<"\n"
             <<"RELR entries="<<result.mod.relrEntries
             <<" targets="<<result.mod.relrTargets<<"\n"
             <<"Read-only validation; no execution or imported code.\n";
    if(!result.mod.ok || result.mod.relrEntries==0 || result.mod.relrTargets==0){
        std::cerr<<"Expected real homebrew MOD0 compact RELR relocations, none decoded.\n";
        return 1;
    }
    const auto image=relrsandbox::copyImage(data.data(),data.size(),result);
    if(!image.ok){std::cerr<<"Failed virtual image copy.\n";return 1;}
    const auto before=image.bytes;
    const auto simulated=relrsandbox::simulate(image,result.mod);
    if(!simulated.ok || simulated.patched!=result.mod.relrTargets ||
       image.bytes!=before || simulated.copy==before){
        std::cerr<<"Real open-source NRO RELR simulation failed: "<<simulated.error<<"\n";
        return 1;
    }
    std::cout<<"PASS: simulated real homebrew RELR on isolated copy: "
             <<simulated.patched<<" relocated addresses, input unchanged\n";
}
