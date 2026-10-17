#include "../app/src/main/cpp/relr_sandbox.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {
nsomemory::Image fixture() {
    nsomemory::Image m;
    m.ok=true;m.base=0;
    m.sectionOffsets={0,0x1000,0x2000};
    m.sectionLengths={0x100,0x100,0x50};
    m.bssOffset=0x2050;m.bssLength=0x20;
    m.bytes.resize(0x2070,0);
    relrsandbox::write64(m.bytes.data()+0x1000,0x2000);
    relrsandbox::write64(m.bytes.data()+0x1008,7); // +8, +16
    relrsandbox::write64(m.bytes.data()+0x2000,0x10);
    relrsandbox::write64(m.bytes.data()+0x2008,0x20);
    relrsandbox::write64(m.bytes.data()+0x2010,0x30);
    return m;
}
modinspect::Result metadata(){
    modinspect::Result m;
    m.ok=true;
    m.relrEntries=2;m.relrTargets=3;
    m.dynamicTags={{36,0x1000},{35,16},{37,8}};
    return m;
}
void pass(const char* label){std::cout<<"PASS: "<<label<<"\n";}
}
int main(){
    auto source=fixture();
    const auto untouched=source.bytes;
    const auto info=metadata();
    const auto result=relrsandbox::simulate(source,info);
    assert(result.ok && result.entries==2 && result.patched==3);
    assert(result.firstTarget==0x2000 && result.firstBefore==0x10 &&
           result.firstAfter==0x100000010ULL);
    pass("direct and two bitmap RELR patches simulated");
    assert(modinspect::u64(result.copy.data()+0x2000)==0x100000010ULL &&
           modinspect::u64(result.copy.data()+0x2008)==0x100000020ULL &&
           modinspect::u64(result.copy.data()+0x2010)==0x100000030ULL);
    pass("all three relocated pointers include mock load bias");
    assert(source.bytes==untouched);
    pass("original reconstructed module remains unchanged");
    auto changed=metadata();
    changed.relrTargets=2;
    assert(!relrsandbox::simulate(source,changed).ok);
    pass("mismatched relocation count rejected");
    changed=metadata();
    changed.dynamicTags[2].second=16;
    assert(!relrsandbox::simulate(source,changed).ok);
    pass("invalid DT_RELRENT rejected");
    auto wrong=source;
    relrsandbox::write64(wrong.bytes.data()+0x1000,0x3000);
    assert(!relrsandbox::simulate(wrong,info).ok);
    pass("relocation address beyond mapped segments rejected");
    wrong=source;
    relrsandbox::write64(wrong.bytes.data()+0x1000,7);
    assert(!relrsandbox::simulate(wrong,info).ok);
    pass("RELR bitmap before a base entry rejected");
    wrong=source;
    relrsandbox::write64(wrong.bytes.data()+0x2000,
                         std::numeric_limits<std::uint64_t>::max());
    assert(!relrsandbox::simulate(wrong,info).ok);
    pass("integer overflow when adding synthetic bias rejected");
    assert(!relrsandbox::simulate(source,info,0).ok);
    pass("zero simulated bias rejected");
    changed=metadata();
    changed.dynamicTags.pop_back();
    assert(!relrsandbox::simulate(source,changed).ok);
    pass("incomplete RELR metadata rejected");
    // Explicitly include a simple NRO mapping and ensure the BSS is initialized.
    std::vector<std::uint8_t> file(0x240,0);
    nroinspect::Info nro;
    nro.ok=true;nro.codeSize=0x240;nro.offsets={0,0x100,0x200};
    nro.lengths={0x100,0x100,0x40};nro.bssSize=0x20;
    file[0x200]=0x5b;
    const auto mapped=relrsandbox::copyImage(file.data(),file.size(),nro);
    assert(mapped.ok && mapped.bytes.size()==0x260 && mapped.bytes[0x200]==0x5b);
    for(std::size_t i=0x240;i<0x260;i++)assert(mapped.bytes[i]==0);
    pass("NRO regions reconstructed as non-executable data and zero BSS");
    std::cout<<"All 11 RELR simulation tests passed.\n";
}
