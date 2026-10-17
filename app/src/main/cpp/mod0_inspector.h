#pragma once
// Bounded, read-only parser for Nintendo Switch MOD0 and ELF64 dynamic metadata.
// No loader, no relocation writes, no code execution, no ROM/keys included.
#include "nso_memory.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace modinspect {
struct Result {
    bool ok=false;
    std::string error;
    std::uint32_t modVa=0, dynamicVa=0;
    std::uint32_t dynamicEntries=0, relaEntries=0, importsFound=0;
    std::uint32_t relative=0, globDat=0, jumpSlot=0, abs64=0, other=0;
    std::vector<std::string> needed;
    std::vector<std::string> importNames;
};
inline std::uint64_t u64(const std::uint8_t* p) {
    std::uint64_t value=0;
    for(int i=0;i<8;i++)value|=std::uint64_t(p[i])<<(i*8);
    return value;
}
inline bool addRelative(std::uint64_t origin,std::uint32_t raw,std::uint64_t& out) {
    const auto offset=static_cast<std::int64_t>(static_cast<std::int32_t>(raw));
    const auto sum=static_cast<std::int64_t>(origin)+offset;
    if(sum<0 || sum>std::numeric_limits<std::uint32_t>::max())return false;
    out=static_cast<std::uint64_t>(sum);
    return true;
}
inline bool contains(const nsomemory::Image& image,std::uint64_t va,
                     std::uint64_t bytes,bool includeBss=false) {
    if(!image.ok || bytes==0 || va>std::numeric_limits<std::uint32_t>::max()
       || bytes>0x100000000ULL-va)return false;
    for(int i=0;i<3;i++) {
        if(!image.sectionLengths[i])continue;
        std::uint64_t start=static_cast<std::uint64_t>(image.base)+image.sectionOffsets[i];
        std::uint64_t end=start+image.sectionLengths[i];
        if(va>=start && va+bytes<=end)return true;
    }
    if(includeBss && image.bssLength) {
        std::uint64_t start=static_cast<std::uint64_t>(image.base)+image.bssOffset;
        if(va>=start && va+bytes<=start+image.bssLength)return true;
    }
    return false;
}
inline const std::uint8_t* at(const nsomemory::Image& img,std::uint64_t va,
                               std::uint64_t size,bool allowBss=false) {
    if(!contains(img,va,size,allowBss) || va<img.base)return nullptr;
    const auto position=va-img.base;
    if(position>img.bytes.size() || size>img.bytes.size()-position)return nullptr;
    return img.bytes.data()+position;
}
inline bool textString(const nsomemory::Image& img,std::uint64_t table,
                       std::uint64_t tableSize,std::uint64_t offset,std::string& result) {
    if(tableSize==0 || tableSize>262144 || offset>=tableSize)return false;
    for(std::uint64_t i=offset;i<tableSize;i++) {
        const auto* p=at(img,table+i,1);
        if(!p)return false;
        if(*p==0)return true;
        if(*p<32 || *p>126 || result.size()>=160)return false;
        result.push_back(static_cast<char>(*p));
    }
    return false;
}
struct Dynamic {
    std::uint64_t strtab=0,strsz=0,symtab=0,syment=0;
    std::uint64_t rela=0,relasz=0,relaent=0;
    std::uint64_t jmprel=0,pltrelsz=0,pltrel=0;
    bool seenStrtab=false,seenStrsz=false,seenSymtab=false,seenSyment=false;
    bool seenRela=false,seenRelasz=false,seenRelaent=false;
    bool seenJmprel=false,seenPltrelsz=false,seenPltrel=false;
    std::vector<std::uint64_t> neededOffsets;
};
inline Result parse(const nsomemory::Image& image) {
    Result result;
    if(!image.ok || image.sectionLengths[0]<0x20){
        result.error="Falta una seccion .text valida.";return result;
    }
    const std::uint64_t textVa=std::uint64_t(image.base)+image.sectionOffsets[0];
    const auto* location=at(image,textVa,8);
    if(!location){result.error="Falta ModuleHeaderLocation al inicio de .text.";return result;}
    std::uint64_t modVa=0;
    if(!addRelative(textVa,nso::read32(location,4),modVa)){
        result.error="Offset MOD0 fuera de rango.";return result;
    }
    const auto* mod=at(image,modVa,0x1c);
    if(!mod || std::memcmp(mod,"MOD0",4)!=0){
        result.error="Firma MOD0 ausente o fuera de .text.";return result;
    }
    const std::uint64_t bssStartRaw=nso::read32(mod,8),bssEndRaw=nso::read32(mod,12);
    std::uint64_t bssStart=0,bssEnd=0,dynamicVa=0;
    if(!addRelative(modVa,nso::read32(mod,4),dynamicVa) ||
       !addRelative(modVa,static_cast<std::uint32_t>(bssStartRaw),bssStart) ||
       !addRelative(modVa,static_cast<std::uint32_t>(bssEndRaw),bssEnd)) {
        result.error="Offsets relativos MOD0 invalidos.";return result;
    }
    // If MOD0 advertises BSS, it should match the reconstructed .data+BSS boundary.
    const std::uint64_t expectedBss=std::uint64_t(image.base)+image.bssOffset;
    if(bssStart>bssEnd ||
       (image.bssLength && (bssStart!=expectedBss ||
          bssEnd!=expectedBss+image.bssLength))) {
        result.error="Limites BSS de MOD0 inconsistentes.";return result;
    }
    result.modVa=static_cast<std::uint32_t>(modVa);
    result.dynamicVa=static_cast<std::uint32_t>(dynamicVa);
    Dynamic d;
    bool terminated=false;
    for(std::uint32_t i=0;i<256;i++) {
        const auto* entry=at(image,dynamicVa+std::uint64_t(i)*16,16);
        if(!entry){result.error="Tabla ELF64 .dynamic truncada.";return result;}
        const std::int64_t tag=static_cast<std::int64_t>(u64(entry));
        const std::uint64_t value=u64(entry+8);
        ++result.dynamicEntries;
        if(tag==0){terminated=true;break;}
        switch(tag) {
            case 1:
                if(d.neededOffsets.size()>=64){result.error="Demasiadas dependencias DT_NEEDED.";return result;}
                d.neededOffsets.push_back(value);break;
            case 5: if(d.seenStrtab){result.error="DT_STRTAB duplicado.";return result;}
                d.seenStrtab=true;d.strtab=value;break;
            case 10: if(d.seenStrsz){result.error="DT_STRSZ duplicado.";return result;}
                d.seenStrsz=true;d.strsz=value;break;
            case 6: if(d.seenSymtab){result.error="DT_SYMTAB duplicado.";return result;}
                d.seenSymtab=true;d.symtab=value;break;
            case 11: if(d.seenSyment){result.error="DT_SYMENT duplicado.";return result;}
                d.seenSyment=true;d.syment=value;break;
            case 7: if(d.seenRela){result.error="DT_RELA duplicado.";return result;}
                d.seenRela=true;d.rela=value;break;
            case 8: if(d.seenRelasz){result.error="DT_RELASZ duplicado.";return result;}
                d.seenRelasz=true;d.relasz=value;break;
            case 9: if(d.seenRelaent){result.error="DT_RELAENT duplicado.";return result;}
                d.seenRelaent=true;d.relaent=value;break;
            case 23: if(d.seenJmprel){result.error="DT_JMPREL duplicado.";return result;}
                d.seenJmprel=true;d.jmprel=value;break;
            case 2: if(d.seenPltrelsz){result.error="DT_PLTRELSZ duplicado.";return result;}
                d.seenPltrelsz=true;d.pltrelsz=value;break;
            case 20: if(d.seenPltrel){result.error="DT_PLTREL duplicado.";return result;}
                d.seenPltrel=true;d.pltrel=value;break;
            default: break; // unknown dynamic tags are safe to record without interpreting
        }
    }
    if(!terminated){result.error="Tabla .dynamic sin DT_NULL en 256 entradas.";return result;}
    if(d.seenStrtab!=d.seenStrsz ||
       ((d.seenSymtab||d.seenSyment) && (!d.seenStrtab || !d.seenSymtab || !d.seenSyment || d.syment!=24))){
        result.error="Tablas de simbolos/cadenas incompletas.";return result;
    }
    if(d.seenStrtab && (d.strsz==0 || d.strsz>262144 || !at(image,d.strtab,d.strsz))){
        result.error="DT_STRTAB/DT_STRSZ fuera de limites.";return result;
    }
    for(auto offset:d.neededOffsets) {
        if(!d.seenStrtab){result.error="DT_NEEDED sin tabla de cadenas.";return result;}
        std::string value;
        if(!textString(image,d.strtab,d.strsz,offset,value) || value.empty()) {
            result.error="Nombre DT_NEEDED fuera de limites.";return result;
        }
        result.needed.push_back(std::move(value));
    }
    auto relocations=[&](std::uint64_t va,std::uint64_t bytes,
                         bool required)->bool {
        if(!required)return true;
        if(bytes==0)return true;
        if(bytes>8192*24ULL || bytes%24 || !at(image,va,bytes)) {
            result.error="Tabla ELF64 RELA fuera de limites o mal alineada.";return false;
        }
        for(std::uint64_t i=0;i<bytes;i+=24) {
            const auto* p=at(image,va+i,24);
            if(!p){result.error="RELA truncada.";return false;}
            const std::uint64_t target=u64(p);
            const std::uint64_t info=u64(p+8);
            const std::uint32_t type=static_cast<std::uint32_t>(info);
            const std::uint32_t symbol=static_cast<std::uint32_t>(info>>32);
            // Only inventory: do not write pointers or patch instructions.
            if(!at(image,target,8,true)) {
                result.error="Destino de reubicacion fuera de memoria.";return false;
            }
            ++result.relaEntries;
            switch(type){
                case 1027: ++result.relative;break; // R_AARCH64_RELATIVE
                case 1025: ++result.globDat;break;  // R_AARCH64_GLOB_DAT
                case 1026: ++result.jumpSlot;break;// R_AARCH64_JUMP_SLOT
                case 257: ++result.abs64;break;    // R_AARCH64_ABS64
                default: ++result.other;break;
            }
            if(symbol && (type==1025 || type==1026 || type==257)) {
                if(!d.seenSymtab || !d.seenStrtab ||
                   symbol>16384 || symbol>std::numeric_limits<std::uint64_t>::max()/24) {
                    result.error="Reubicacion con simbolo sin DT_SYMTAB valido.";return false;
                }
                const auto* sym=at(image,d.symtab+std::uint64_t(symbol)*24,24);
                if(!sym){result.error="Simbolo ELF64 fuera de limites.";return false;}
                const std::uint32_t nameOffset=nso::read32(sym,0);
                std::string name;
                if(!textString(image,d.strtab,d.strsz,nameOffset,name) || name.empty()){
                    result.error="Nombre de simbolo fuera de limites.";return false;
                }
                if(result.importNames.size()<48) result.importNames.push_back(std::move(name));
                ++result.importsFound;
            }
        }
        return true;
    };
    if(d.seenRela!=d.seenRelasz || (d.seenRela && (!d.seenRelaent || d.relaent!=24))){
        result.error="Tags DT_RELA incompletos.";return result;
    }
    if(!relocations(d.rela,d.relasz,d.seenRela))return result;
    if(d.seenJmprel!=d.seenPltrelsz ||
        (d.seenJmprel && (!d.seenPltrel || d.pltrel!=7))){
        result.error="DT_JMPREL requiere DT_PLTREL=RELA.";return result;
    }
    if(!relocations(d.jmprel,d.pltrelsz,d.seenJmprel))return result;
    result.ok=true;
    return result;
}
inline std::string report(const Result& r) {
    if(!r.ok)return "MOD0 rechazado: "+r.error;
    std::string s="MOD0 / ELF64 reconocido (SOLO LECTURA)\n";
    s+="MOD0 virtual: "+std::to_string(r.modVa)+
       "\n.dynamic virtual: "+std::to_string(r.dynamicVa)+
       "\nEntradas dinamicas: "+std::to_string(r.dynamicEntries)+
       "\nRelocaciones RELA: "+std::to_string(r.relaEntries);
    s+="\nRELATIVE: "+std::to_string(r.relative)+
       " | GLOB_DAT: "+std::to_string(r.globDat)+
       "\nJUMP_SLOT: "+std::to_string(r.jumpSlot)+
       " | ABS64: "+std::to_string(r.abs64)+
       " | Otros: "+std::to_string(r.other);
    s+="\nSimbolos externos encontrados: "+std::to_string(r.importsFound);
    for(const auto& library:r.needed)s+="\nNecesita: "+library;
    for(const auto& name:r.importNames)s+="\nImportacion: "+name;
    s+="\n\nNO se aplican relocaciones, no se ejecuta codigo ni se cargan ROMs.";
    return s;
}
} // namespace modinspect
