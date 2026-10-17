#pragma once
// Safe, bounded reconstruction of a synthetic NSO0 memory image.
// This loader does NOT execute instructions or reproduce Nintendo Switch services.
#include "nso_format.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace nsomemory {
constexpr std::uint64_t kMaxMappedBytes=16ULL*1024*1024;
constexpr std::uint64_t kMaxSegmentBytes=8ULL*1024*1024;
constexpr std::uint64_t kMaxBssBytes=4ULL*1024*1024;
struct Image {
    bool ok=false;
    std::string error;
    std::uint32_t base=0;
    std::array<std::uint32_t,3> sectionOffsets{};
    std::array<std::uint32_t,3> sectionLengths{};
    std::uint32_t bssOffset=0;
    std::uint32_t bssLength=0;
    std::vector<std::uint8_t> bytes;
};

inline bool lz4Block(const std::uint8_t* src,std::size_t inputSize,
                     std::uint8_t* output,std::size_t outputSize) {
    // LZ4 raw block format, no legacy framing, no allocations, exact output length.
    std::size_t in=0,out=0;
    while (in<inputSize) {
        const std::uint8_t token=src[in++];
        std::size_t literal=token>>4;
        if(literal==15) {
            while(true) {
                if(in>=inputSize)return false;
                auto add=src[in++];
                if(literal>outputSize || add>outputSize-literal)return false;
                literal+=add;
                if(add!=255)break;
            }
        }
        if(literal>inputSize-in || literal>outputSize-out)return false;
        if(literal) {std::memcpy(output+out,src+in,literal);in+=literal;out+=literal;}
        if(in==inputSize)return out==outputSize;
        if(inputSize-in<2)return false;
        const std::size_t back=static_cast<std::size_t>(src[in]) |
                                   (static_cast<std::size_t>(src[in+1])<<8);
        in+=2;
        if(back==0 || back>out)return false;
        std::size_t match=static_cast<std::size_t>(token & 0xf)+4;
        if((token & 0xf)==15) {
            while(true) {
                if(in>=inputSize)return false;
                auto add=src[in++];
                if(match>outputSize || add>outputSize-match)return false;
                match+=add;
                if(add!=255)break;
            }
        }
        if(match>outputSize-out)return false;
        // Matches overlap by design; copying forward byte-by-byte is mandatory.
        for(std::size_t i=0;i<match;i++) output[out+i]=output[out+i-back];
        out+=match;
    }
    return out==outputSize;
}

inline Image reconstruct(const std::uint8_t* file,std::size_t fileSize) {
    Image image;
    const auto info=nso::inspect(file,std::min(fileSize,nso::kHeaderSize),
                                 static_cast<std::uint64_t>(fileSize));
    if(!info.valid){image.error=info.reason;return image;}
    std::uint64_t lo=std::numeric_limits<std::uint64_t>::max(),hi=0,total=0;
    bool present=false;
    for(int i=0;i<3;i++){
        auto s=info.sections[i];
        if(!s.memSize)continue;
        present=true;
        if(s.memSize>kMaxSegmentBytes){image.error="Segmento demasiado grande para el laboratorio.";return image;}
        total+=s.memSize;
        lo=std::min(lo,static_cast<std::uint64_t>(s.memOffset));
        hi=std::max(hi,static_cast<std::uint64_t>(s.memOffset)+s.memSize);
        for(int j=i+1;j<3;j++){
            auto t=info.sections[j];
            if(!t.memSize)continue;
            auto a=static_cast<std::uint64_t>(s.memOffset),b=static_cast<std::uint64_t>(t.memOffset);
            if(a<b+t.memSize && b<a+s.memSize){
                image.error="Segmentos superpuestos en memoria virtual.";return image;
            }
        }
    }
    if(!present){image.error="El NSO no contiene segmentos.";return image;}
    if(info.bssSize>kMaxBssBytes){image.error="BSS demasiado grande.";return image;}
    // BSS starts immediately after the .data segment, not at arbitrary file offsets.
    const auto data=info.sections[2];
    const std::uint64_t bssStart=static_cast<std::uint64_t>(data.memOffset)+data.memSize;
    const std::uint64_t bssEnd=bssStart+info.bssSize;
    if(info.bssSize){
        if(!data.memSize){image.error="BSS requiere segmento .data.";return image;}
        for(int i=0;i<2;i++){
            const auto s=info.sections[i];
            if(s.memSize && bssStart<static_cast<std::uint64_t>(s.memOffset)+s.memSize
               && static_cast<std::uint64_t>(s.memOffset)<bssEnd) {
                image.error="BSS superpuesto con otro segmento.";return image;
            }
        }
        hi=std::max(hi,bssEnd);
    }
    if(hi<lo || hi-lo>kMaxMappedBytes || total+info.bssSize>kMaxMappedBytes){
        image.error="Imagen de memoria supera 16 MiB.";return image;
    }
    image.base=static_cast<std::uint32_t>(lo);
    image.bytes.resize(static_cast<std::size_t>(hi-lo),0);
    for(int i=0;i<3;i++){
        const auto s=info.sections[i];
        image.sectionOffsets[i]=s.memOffset-image.base;
        image.sectionLengths[i]=s.memSize;
        if(!s.memSize)continue;
        auto* dest=image.bytes.data()+image.sectionOffsets[i];
        auto* src=file+s.fileOffset;
        if(s.compressed){
            if(!lz4Block(src,s.storedSize,dest,s.memSize)){
                image.error="Bloque LZ4 invalido o tamano descomprimido incorrecto.";
                image.bytes.clear();return image;
            }
        }else{
            std::memcpy(dest,src,s.memSize);
        }
    }
    image.bssLength=info.bssSize;
    if(info.bssSize)image.bssOffset=static_cast<std::uint32_t>(bssStart-lo);
    image.ok=true;
    return image;
}
} // namespace nsomemory
