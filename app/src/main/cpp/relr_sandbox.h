#pragma once
// Simulate ELF64 RELR on a NON-EXECUTABLE copy of a homebrew NRO memory image.
// No mprotect, no PROT_EXEC, no external library loading, no Nintendo services.
#include "nro_inspector.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace relrsandbox {
constexpr std::uint64_t kSimulatedBias = 0x100000000ULL; // marker, not actual mapped address
constexpr std::uint64_t kMaxEntries = 8192;
constexpr std::uint64_t kMaxTargets = kMaxEntries * 63;

struct Result {
    bool ok=false;
    std::string error;
    std::uint64_t simulatedBase=kSimulatedBias;
    std::uint32_t entries=0, patched=0;
    std::uint64_t firstTarget=0, firstBefore=0, firstAfter=0;
    std::vector<std::uint8_t> copy; // private, data-only buffer
};
inline void write64(std::uint8_t* dest,std::uint64_t value) {
    for(int i=0;i<8;i++)dest[i]=static_cast<std::uint8_t>(value>>(i*8));
}
inline bool tagValue(const modinspect::Result& meta,
                     std::int64_t tag,std::uint64_t& out) {
    bool found=false;
    for(const auto& pair:meta.dynamicTags) {
        if(pair.first!=tag)continue;
        if(found)return false;
        out=pair.second;found=true;
    }
    return found;
}
// Retains the same virtual section boundaries as the validated NRO inspector.
inline nsomemory::Image copyImage(const std::uint8_t* file,std::size_t size,
                                   const nroinspect::Info& info) {
    nsomemory::Image image;
    if(!file || !info.ok || size<info.codeSize || size>nroinspect::kMaxInput) {
        image.error="Entrada NRO invalida.";return image;
    }
    std::uint64_t end=0;
    for(int i=0;i<3;i++){
        const std::uint64_t at=info.offsets[i], len=info.lengths[i];
        if(!len || !nroinspect::range(at,len,info.codeSize)) {
            image.error="Seccion NRO fuera de limites.";return image;
        }
        end=std::max(end,at+len);
        image.sectionOffsets[i]=info.offsets[i];
        image.sectionLengths[i]=info.lengths[i];
    }
    if(end+info.bssSize>nroinspect::kMaxMemory) {
        image.error="Memoria virtual excede limite de laboratorio.";return image;
    }
    image.base=0;
    image.bssOffset=static_cast<std::uint32_t>(end);
    image.bssLength=info.bssSize;
    image.bytes.resize(static_cast<std::size_t>(end+info.bssSize),0);
    for(int i=0;i<3;i++)
        std::copy_n(file+info.offsets[i],info.lengths[i],
                    image.bytes.data()+info.offsets[i]);
    image.ok=true;
    return image;
}
inline Result simulate(const nsomemory::Image& source,const modinspect::Result& meta,
                       std::uint64_t bias=kSimulatedBias) {
    Result result;
    result.simulatedBase=bias;
    if(!source.ok || !meta.ok || source.bytes.empty() || source.bytes.size()>nroinspect::kMaxMemory) {
        result.error="Memoria o metadatos no validados.";return result;
    }
    if(bias==0) {result.error="La base simulada debe ser distinta de cero.";return result;}
    std::uint64_t addr=0,size=0,stride=0;
    if(!tagValue(meta,36,addr) || !tagValue(meta,35,size) || !tagValue(meta,37,stride) ||
       stride!=8 || size==0 || size%8 || size/8>kMaxEntries ||
       !modinspect::at(source,addr,size)) {
        result.error="Tabla RELR no disponible o invalida.";return result;
    }
    // Stage and validate all address changes before modifying the copy.
    struct Edit {std::size_t offset;std::uint64_t oldValue;std::uint64_t newValue;};
    std::vector<Edit> edits;
    std::uint64_t next=0;
    bool haveBase=false;
    auto stage=[&](std::uint64_t va)->bool{
        if((va&7)!=0 || !modinspect::at(source,va,8,true) || va<source.base)
            return false;
        const std::uint64_t offset=va-source.base;
        if(offset>source.bytes.size() || source.bytes.size()-offset<8)return false;
        if(edits.size()>=kMaxTargets)return false;
        const auto old=modinspect::u64(source.bytes.data()+offset);
        if(old>std::numeric_limits<std::uint64_t>::max()-bias)return false;
        // Reject a duplicate (RELR addresses should be strictly increasing).
        if(!edits.empty() && offset<=edits.back().offset)return false;
        edits.push_back({static_cast<std::size_t>(offset),old,old+bias});
        return true;
    };
    for(std::uint64_t i=0;i<size/8;i++){
        const auto* pointer=modinspect::at(source,addr+i*8,8);
        if(!pointer){result.error="RELR truncada.";return result;}
        const std::uint64_t word=modinspect::u64(pointer);
        ++result.entries;
        if((word&1)==0){
            if(!stage(word) || word>std::numeric_limits<std::uint64_t>::max()-8) {
                result.error="Destino RELR directo invalido.";return result;
            }
            next=word+8;haveBase=true;
        } else {
            if(!haveBase || next>std::numeric_limits<std::uint64_t>::max()-63*8) {
                result.error="Bitmap RELR sin base valida.";return result;
            }
            const auto bitmap=word>>1;
            for(std::uint64_t bit=0;bit<63;bit++) {
                if((bitmap&(1ULL<<bit)) && !stage(next+bit*8)){
                    result.error="Destino RELR de bitmap invalido.";return result;
                }
            }
            next+=63*8;
        }
    }
    if(result.entries!=meta.relrEntries || edits.size()!=meta.relrTargets){
        result.error="Conteo RELR no coincide con el inspector.";return result;
    }
    result.copy=source.bytes;
    for(const auto& edit:edits)
        write64(result.copy.data()+edit.offset,edit.newValue);
    result.patched=static_cast<std::uint32_t>(edits.size());
    if(!edits.empty()){
        result.firstTarget=static_cast<std::uint64_t>(edits.front().offset)+source.base;
        result.firstBefore=edits.front().oldValue;
        result.firstAfter=edits.front().newValue;
    }
    result.ok=true;
    return result;
}
inline std::string report(const Result& r) {
    if(!r.ok)return "Simulacion RELR rechazada: "+r.error;
    return "Simulacion RELR en copia aislada: CORRECTA\n"
         "Base ficticia: "+std::to_string(r.simulatedBase)+
         "\nEntradas RELR: "+std::to_string(r.entries)+
         "\nPunteros ajustados en COPIA: "+std::to_string(r.patched)+
         "\nPrimer destino virtual: "+std::to_string(r.firstTarget)+
         "\nPrimer valor original: "+std::to_string(r.firstBefore)+
         "\nPrimer valor simulado: "+std::to_string(r.firstAfter)+
         "\n\nNo modifica el archivo ni ejecuta instrucciones; "
         "no es un cargador funcional de Switch.";
}
} // namespace relrsandbox
