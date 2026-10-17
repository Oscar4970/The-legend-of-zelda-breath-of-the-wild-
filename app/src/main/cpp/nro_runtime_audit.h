#pragma once
// Switch NRO Homebrew-ABI and instruction compatibility preflight.
// ALL OPERATIONS ARE READ ONLY. Never uses mmap(PROT_EXEC) or invokes loaded code.
// Definitions verified against switchbrew.org/wiki/Homebrew_ABI and /wiki/SVC.
#include "nro_inspector.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace runtimeaudit {
struct ConfigEntry {
    std::uint32_t key;
    std::uint32_t flags;
    std::uint64_t values[2];
};
static_assert(sizeof(ConfigEntry)==24, "Switch Homebrew ABI requires 24 byte config entries");
static_assert(offsetof(ConfigEntry,values)==8, "Switch Homebrew ABI values are at +8");
constexpr std::uint64_t kNroSecondRegister=0xffffffffffffffffULL;
constexpr std::size_t kMaxConfigEntries=32;
struct ConfigResult {
    bool ok=false;
    std::string reason;
    std::uint32_t entries=0;
};
// This verifies ABI data layout ONLY; it does not fabricate Horizon handles.
inline ConfigResult validateConfig(const ConfigEntry* entries,std::size_t count) {
    ConfigResult out;
    if(!entries || count==0 || count>kMaxConfigEntries) {
        out.reason="Configuracion ABI ausente o demasiado larga.";return out;
    }
    bool seenMain=false,seenApplet=false,seenEnd=false;
    std::uint32_t used=0;
    for(std::size_t i=0;i<count;i++){
        const auto& e=entries[i];
        if((e.flags & ~1u)!=0){
            out.reason="Flags de LoaderConfig no reconocidos.";return out;
        }
        if(e.key==0) {
            if(i+1!=count){
                out.reason="EndOfList debe ser el ultimo elemento.";return out;
            }
            seenEnd=true;break;
        }
        // Only keys we can IDENTIFY, not necessarily safely IMPLEMENT.
        if(e.key>17 || e.key==9){
            if(e.flags&1u){
                out.reason="Entrada obligatoria desconocida en LoaderConfig.";return out;
            }
        }
        if(e.key==1){
            if(seenMain || e.values[0]==0 || e.values[1]!=0){
                out.reason="MainThreadHandle duplicado o invalido.";return out;
            }
            seenMain=true;
        }
        if(e.key==7){
            if(seenApplet || e.values[0]>4){
                out.reason="AppletType duplicado o invalido.";return out;
            }
            seenApplet=true;
        }
        ++used;
    }
    if(!seenEnd || !seenMain || !seenApplet){
        out.reason="Faltan EndOfList, MainThreadHandle o AppletType.";return out;
    }
    out.ok=true;out.entries=used;
    return out;
}
struct Report {
    bool nroValid=false;
    bool modParsed=false;
    bool entryBranch=false;
    bool entryTargetInsideText=false;
    std::int64_t entryTarget=-1;
    std::uint32_t scannedWords=0;
    std::uint32_t svcSites=0,brkSites=0;
    std::vector<std::uint16_t> svcIds;
    std::vector<std::string> missing;
    std::string summary;
};
inline std::uint32_t little32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0])|
           static_cast<std::uint32_t>(p[1])<<8|
           static_cast<std::uint32_t>(p[2])<<16|
           static_cast<std::uint32_t>(p[3])<<24;
}
inline const char* svcName(std::uint16_t id) {
    switch(id){
      case 1:return "SetHeapSize";
      case 2:return "SetMemoryPermission";
      case 6:return "QueryMemory";
      case 7:return "ExitProcess";
      case 8:return "CreateThread";
      case 9:return "StartThread";
      case 10:return "ExitThread";
      case 11:return "SleepThread";
      default:return "requiere consultar Horizon SVC";
    }
}
inline Report examine(const std::uint8_t* bytes,std::size_t count) {
    Report r;
    const auto nro=nroinspect::inspect(bytes,count);
    r.nroValid=nro.ok;
    if(!nro.ok){r.summary="NRO rechazado: "+nro.error;return r;}
    r.modParsed=nro.mod.ok;
    if(!r.modParsed)
        r.missing.push_back("MOD0 no analizable: "+nro.mod.error);
    else {
        if(!nro.mod.needed.empty() || nro.mod.importsFound)
            r.missing.push_back("Resolver bibliotecas y simbolos de Switch");
        if(nro.mod.relEntries || nro.mod.relaEntries || nro.mod.relrEntries)
            r.missing.push_back("Aplicar REL, RELA y RELR en memoria del cargador, con protecciones W^X");
    }
    const auto textStart=static_cast<std::size_t>(nro.offsets[0]);
    const auto textEnd=textStart+nro.lengths[0];
    if(textEnd>count || textStart>textEnd || textEnd-textStart<0x80){
        r.summary="Seccion .text fuera del archivo.";
        r.nroValid=false;return r;
    }
    const std::uint32_t first=little32(bytes+textStart);
    // NRO startup entry at virtual .text+0 must branch past the header.
    r.entryBranch=(first&0xfc000000u)==0x14000000u;
    if(r.entryBranch){
        std::int32_t imm=static_cast<std::int32_t>(first&0x03ffffffu);
        if(imm&0x02000000)imm|=static_cast<std::int32_t>(~0x03ffffffu);
        r.entryTarget=static_cast<std::int64_t>(textStart)+static_cast<std::int64_t>(imm)*4;
        r.entryTargetInsideText=r.entryTarget>=static_cast<std::int64_t>(textStart+0x80)
            && r.entryTarget+4<=static_cast<std::int64_t>(textEnd);
    }
    if(!r.entryTargetInsideText)
        r.missing.push_back("Punto de entrada AArch64 incompatible o no validado");
    // Heuristic static scan; data/inline constants can resemble instructions.
    // This is NOT a disassembler or a proof of reachable SVC calls.
    for(std::size_t offset=textStart+0x80;offset+4<=textEnd;offset+=4){
        const auto word=little32(bytes+offset);
        ++r.scannedWords;
        if((word&0xffe0001fu)==0xd4000001u){
            ++r.svcSites;
            const auto id=static_cast<std::uint16_t>((word>>5)&0xffffu);
            if(std::find(r.svcIds.begin(),r.svcIds.end(),id)==r.svcIds.end() &&
               r.svcIds.size()<64)
                r.svcIds.push_back(id);
        }else if((word&0xffe0001fu)==0xd4200000u){
            ++r.brkSites;
        }
    }
    if(r.svcSites)
        r.missing.push_back("Traducir SVC de Horizon: no pueden ejecutarse como syscalls Android");
    r.missing.push_back("Proporcionar LoaderConfig con handles y AppletType verdaderos o compatibles");
    r.missing.push_back("Preparar TLS, stack, heap y gestion segura de salida del NRO");
    r.missing.push_back("Aislar fallos antes de permitir codigo ARM64 externo");
    r.summary="Prefiltro de compatibilidad finalizado. NO se ejecuto ninguna instruccion.";
    return r;
}
inline std::string display(const Report& r) {
    if(!r.nroValid)return r.summary;
    std::string msg="PREFILTRO NRO ARM64 (SOLO LECTURA)\n";
    msg+="Salto de entrada NRO: "+std::string(r.entryBranch?"detectado":"no detectado");
    msg+="\nDestino de entrada: "+std::to_string(r.entryTarget);
    msg+="\nDestino dentro de .text: "+std::string(r.entryTargetInsideText?"si":"no");
    msg+="\nMOD0: "+std::string(r.modParsed?"analizado":"no analizado");
    msg+="\nPalabras escaneadas: "+std::to_string(r.scannedWords);
    msg+="\nPatrones SVC detectados: "+std::to_string(r.svcSites);
    msg+="\nPatrones BRK: "+std::to_string(r.brkSites);
    for(const auto id:r.svcIds)
        msg+="\n  SVC #"+std::to_string(id)+" ("+svcName(id)+")";
    msg+="\n\nCOMPONENTES AUN PENDIENTES:";
    for(const auto& issue:r.missing)msg+="\n- "+issue;
    msg+="\n\n"+r.summary+
         "\nEl escaneo de SVC es heuristico, no confirma que las instrucciones sean alcanzables.";
    return msg;
}
} // namespace runtimeaudit
