#pragma once
// Read-only, size-bounded NRO0 inspection for independently obtained Switch homebrew.
// Does NOT execute code, decrypt content, load services, or translate graphics.
#include "mod0_inspector.h"
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace nroinspect {
constexpr std::uint64_t kMaxInput = 16ULL * 1024 * 1024;
constexpr std::uint64_t kMaxMemory = 16ULL * 1024 * 1024;
struct Asset {
    bool present=false;
    std::uint64_t offset=0,size=0;
};
struct Info {
    bool ok=false;
    std::string error;
    std::uint32_t version=0,flags=0,codeSize=0,bssSize=0;
    std::array<std::uint32_t,3> offsets{}, lengths{};
    std::string moduleId;
    bool assetsPresent=false;
    std::array<Asset,3> assets{};
    bool modAttempted=false;
    modinspect::Result mod{};
};
inline std::uint32_t u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0])
        | (static_cast<std::uint32_t>(p[1])<<8)
        | (static_cast<std::uint32_t>(p[2])<<16)
        | (static_cast<std::uint32_t>(p[3])<<24);
}
inline std::uint64_t u64(const std::uint8_t* p) {
    std::uint64_t result=0;
    for(int i=0;i<8;i++) result|=static_cast<std::uint64_t>(p[i])<<(8*i);
    return result;
}
inline bool range(std::uint64_t at,std::uint64_t size,std::uint64_t limit) {
    return at<=limit && size<=limit-at;
}
inline Info inspect(const std::uint8_t* file,std::size_t size) {
    Info out;
    if(!file || size<0x80 || size>kMaxInput){
        out.error="El archivo NRO tiene un tamano no admitido (minimo 128 B, maximo 16 MiB).";
        return out;
    }
    if(std::memcmp(file+0x10,"NRO0",4)!=0) {
        out.error="No se encontro la firma NRO0 en 0x10.";
        return out;
    }
    out.version=u32(file+0x14);
    out.codeSize=u32(file+0x18);
    out.flags=u32(file+0x1c);
    out.bssSize=u32(file+0x38);
    if((out.flags & 2)!=0){
        out.error="NRO comprimido moderno aun no compatible con esta version.";
        return out;
    }
    if(out.codeSize<0x80 || out.codeSize>size){
        out.error="Tamano declarado del NRO excede el archivo.";
        return out;
    }
    constexpr std::size_t positions[3]={0x20,0x28,0x30};
    std::uint64_t top=0;
    for(int i=0;i<3;i++){
        out.offsets[i]=u32(file+positions[i]);
        out.lengths[i]=u32(file+positions[i]+4);
        if(out.lengths[i]==0){
            out.error="Seccion NRO vacia o incompleta.";
            return out;
        }
        if(!range(out.offsets[i],out.lengths[i],out.codeSize)){
            out.error="Seccion NRO fuera del tamano declarado.";
            return out;
        }
        // NRO's .text contains its first 0x80 bytes of header and startup code.
        if(i==0 && (out.offsets[i]!=0 || out.lengths[i]<0x80)){
            out.error="Seccion .text incompatible con la cabecera NRO.";
            return out;
        }
        if(i>0 && static_cast<std::uint64_t>(out.offsets[i])<top){
            out.error="Secciones NRO solapadas o desordenadas.";
            return out;
        }
        top=static_cast<std::uint64_t>(out.offsets[i])+out.lengths[i];
    }
    if(out.bssSize>4*1024*1024 || top+out.bssSize>kMaxMemory) {
        out.error="Imagen NRO excede los limites de memoria del laboratorio.";
        return out;
    }
    // Module ID is descriptive metadata, not proof of origin or authenticity.
    constexpr char digits[]="0123456789abcdef";
    for(int i=0;i<32;i++){
        auto value=file[0x40+i];
        out.moduleId.push_back(digits[value>>4]);
        out.moduleId.push_back(digits[value&15]);
    }
    // Homebrew often appends an ASET block after the NRO code image.
    if(range(out.codeSize,0x38,size) &&
       std::memcmp(file+out.codeSize,"ASET",4)==0) {
        out.assetsPresent=true;
        const auto aset=static_cast<std::uint64_t>(out.codeSize);
        for(int i=0;i<3;i++){
            auto offset=u64(file+out.codeSize+8+16*i);
            auto length=u64(file+out.codeSize+16+16*i);
            if(length==0)continue;
            if(offset<0x38 || offset>size-aset || !range(aset+offset,length,size)) {
                out.error="Recurso ASET invalido o fuera del archivo.";
                out.assetsPresent=false;return out;
            }
            out.assets[i]={true,offset,length};
        }
    }
    // Build a NON-executable copy of the NRO's virtual sections for MOD0 analysis.
    nsomemory::Image image;
    image.ok=true;
    image.base=0;
    image.hashChecks=0; // NRO0 has no per-section NSO SHA-256 verification
    for(int i=0;i<3;i++){
        image.sectionOffsets[i]=out.offsets[i];
        image.sectionLengths[i]=out.lengths[i];
    }
    image.bssOffset=static_cast<std::uint32_t>(top);
    image.bssLength=out.bssSize;
    image.bytes.assign(static_cast<std::size_t>(top+out.bssSize),0);
    for(int i=0;i<3;i++)
        std::memcpy(image.bytes.data()+out.offsets[i],
                    file+out.offsets[i],out.lengths[i]);
    out.modAttempted=true;
    out.mod=modinspect::parse(image);
    out.ok=true;
    return out;
}
inline std::string report(const Info& data) {
    if(!data.ok)return "NRO rechazado: "+data.error;
    std::string s="NRO0 reconocido (SOLO LECTURA)\n";
    s+="Version: "+std::to_string(data.version);
    s+="\nTamano de ejecutable: "+std::to_string(data.codeSize)+" bytes";
    s+="\n.text: "+std::to_string(data.lengths[0])+" bytes";
    s+="\n.rodata: "+std::to_string(data.lengths[1])+" bytes";
    s+="\n.data: "+std::to_string(data.lengths[2])+" bytes";
    s+="\nBSS: "+std::to_string(data.bssSize)+" bytes";
    s+="\nModule ID: "+data.moduleId;
    if(data.assetsPresent){
        static constexpr const char* names[3]={"Icono","NACP","RomFS"};
        s+="\nRecursos ASET:";
        for(int i=0;i<3;i++)
            if(data.assets[i].present)
                s+="\n  "+std::string(names[i])+": "+std::to_string(data.assets[i].size)+" bytes";
    }
    if(data.mod.ok){
        s+="\n\n"+modinspect::report(data.mod);
    } else if(data.modAttempted) {
        s+="\n\nMOD0 no analizable: "+data.mod.error;
    }
    s+="\n\nNo ejecuta programas ni autentica el archivo.";
    return s;
}
} // namespace nroinspect
