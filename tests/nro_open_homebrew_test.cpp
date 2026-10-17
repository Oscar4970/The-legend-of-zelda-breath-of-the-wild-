// Integration check against an independently published open-source Switch NRO.
// This program only reads and prints metadata; it NEVER executes NRO instructions.
#include "../app/src/main/cpp/nro_inspector.h"
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
    std::cout<<"PASS: public independent open-source NRO0, "
             <<data.size()<<" source bytes\n"
             <<"Text="<<result.lengths[0]<<" Ro="<<result.lengths[1]
             <<" Data="<<result.lengths[2]<<" BSS="<<result.bssSize<<"\n"
             <<"ASET present="<<(result.assetsPresent?"yes":"no")<<"\n"
             <<"MOD0="<<(result.mod.ok?"parsed":"not parsed")
             <<(result.mod.ok?"":": "+result.mod.error)<<"\n"
             <<"Read-only validation; no execution or imported code.\n";
}
