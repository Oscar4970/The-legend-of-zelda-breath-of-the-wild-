// Read-only, bounded Switch ROM explorer: NSP/PFS0, XCI/HFS0, NCA, NSO and NRO.
// Operates on caller-selected local file descriptors. Never decrypts, extracts,
// executes or uploads game content. Works with synthetic test fixtures in CI.
// Format specs: switchbrew.org/wiki/XCI, switchbrew.org/wiki/NCA, switchbrew.org/wiki/NSO0
#pragma once
#include "switch_container_inspector.h"
#include "nso_format.h"
#include "nro_inspector.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace switchdeep {

constexpr uint32_t kMaxEntries=20000;
constexpr uint32_t kMaxNamesBytes=2u*1024u*1024u;
constexpr uint32_t kListedEntries=28;
constexpr uint64_t kMaxModuleRead=16ull*1024*1024;
constexpr size_t kMaxReportChars=15000;

struct Entry {
    std::string name;
    uint64_t absolute=0;
    uint64_t bytes=0;
    uint32_t hashed=0;
};
struct Directory {
    bool ok=false;
    std::string error;
    std::vector<Entry> entries;
};
struct Summary {
    uint32_t nca=0, encryptedOrUnknown=0, readableNca=0, program=0, meta=0, control=0;
    uint32_t executables=0, tickets=0, certificates=0, other=0;
    uint64_t payloadBytes=0;
};

inline bool within(uint64_t pos,uint64_t len,uint64_t end) {
    return pos<=end && len<=end-pos;
}
inline bool add(uint64_t a,uint64_t b,uint64_t& v) {
    if(b>UINT64_MAX-a)return false;
    v=a+b;
    return true;
}
inline bool suffix(const std::string& text,const char* ext) {
    const size_t n=std::strlen(ext);
    if(text.size()<n)return false;
    for(size_t i=0;i<n;i++){
        auto c=static_cast<unsigned char>(text[text.size()-n+i]);
        if(c>='A'&&c<='Z')c=static_cast<unsigned char>(c-'A'+'a');
        if(c!=static_cast<unsigned char>(ext[i]))return false;
    }
    return true;
}
inline const char* ncaType(unsigned char c) {
    switch(c) {
        case 0:return "PROGRAM (candidato a codigo)";
        case 1:return "META";
        case 2:return "CONTROL";
        case 3:return "MANUAL";
        case 4:return "DATA";
        case 5:return "PUBLIC_DATA";
        default:return "DESCONOCIDO";
    }
}
inline std::string sizes(uint64_t n) {
    std::ostringstream s;
    s<<n<<" bytes";
    if(n>=1024*1024)s<<" ("<<std::fixed<<std::setprecision(1)
            <<(double(n)/(1024.0*1024.0))<<" MiB)";
    return s.str();
}
inline bool read(int fd, void* p,size_t amount,uint64_t where) {
    return switchfile::readAt(fd,p,amount,where);
}
inline std::string error(const std::string& why) {
    return "ANALISIS DE ROM — ARCHIVO RECHAZADO\n"+why+
           "\nNo se modifico ningun dato ni se envio a internet.";
}
inline Directory parseDirectory(int fd,uint64_t root,uint64_t region,
                                uint64_t fileSize,bool hfs0) {
    Directory result;
    uint64_t limit=0;
    if(!within(root,16,fileSize) || !add(root,region,limit) || limit>fileSize) {
        result.error="Region del contenedor fuera del archivo.";return result;
    }
    std::array<unsigned char,16> head{};
    if(!read(fd,head.data(),head.size(),root) ||
        std::memcmp(head.data(),hfs0?"HFS0":"PFS0",4)!=0) {
        result.error="Falta firma "+std::string(hfs0?"HFS0":"PFS0")+
                     " en la posicion declarada.";return result;
    }
    const auto count=switchfile::read32(head.data()+4);
    const auto strings=switchfile::read32(head.data()+8);
    const uint64_t recordSize=hfs0?64:24;
    if(count>kMaxEntries || strings>kMaxNamesBytes) {
        result.error="Demasiadas entradas o nombres (>limite de seguridad).";return result;
    }
    const uint64_t tableBytes=uint64_t(count)*recordSize;
    const uint64_t dataBegin=16+tableBytes+strings;
    if(dataBegin>region){
        result.error="Indice o tabla de nombres truncados.";return result;
    }
    std::vector<unsigned char> raw(static_cast<size_t>(tableBytes));
    std::vector<unsigned char> names(static_cast<size_t>(strings));
    if((tableBytes&&!read(fd,raw.data(),raw.size(),root+16)) ||
       (strings&&!read(fd,names.data(),names.size(),root+16+tableBytes))) {
        result.error="No se pueden leer los metadatos del indice.";return result;
    }
    // The parser ignores user-supplied paths for filesystem access; they are labels only.
    result.entries.reserve(count);
    for(uint32_t i=0;i<count;i++) {
        const auto* p=raw.data()+size_t(i)*size_t(recordSize);
        const auto off=switchfile::read64(p);
        const auto len=switchfile::read64(p+8);
        const auto namePos=switchfile::read32(p+16);
        const auto hashed=hfs0?switchfile::read32(p+20):0;
        if(namePos>=strings || off>region-dataBegin ||
           len>region-dataBegin-off || (hfs0 && hashed>len)) {
            result.error="Entrada "+std::to_string(i)+" fuera de limites.";return result;
        }
        const auto* begin=names.data()+namePos;
        const auto* end=static_cast<const unsigned char*>(
            std::memchr(begin,0,strings-namePos));
        if(!end || end==begin || end-begin>255) {
            result.error="Nombre invalido en entrada "+std::to_string(i)+".";return result;
        }
        std::string name;
        for(auto* q=begin;q<end;q++){
            if(*q<32 || *q>126 || *q=='/' || *q=='\\'){
                result.error="Nombre de indice no admitido.";return result;
            }
            name.push_back(static_cast<char>(*q));
        }
        uint64_t absolute=0;
        if(!add(root+dataBegin,off,absolute) || !within(absolute,len,limit)) {
            result.error="Desbordamiento de offset del contenedor.";return result;
        }
        result.entries.push_back({name,absolute,len,hashed});
    }
    result.ok=true;
    return result;
}

inline std::string reportNca(int fd,const Entry& e,Summary& summary) {
    ++summary.nca;
    if(e.bytes<0x224){
        ++summary.encryptedOrUnknown;
        return "NCA demasiado pequeno para inspeccionar cabecera.";
    }
    std::array<unsigned char,0x224> header{};
    if(!read(fd,header.data(),header.size(),e.absolute)){
        ++summary.encryptedOrUnknown;
        return "Cabecera NCA inaccesible.";
    }
    const auto* marker=header.data()+0x200;
    if(std::memcmp(marker,"NCA3",4)!=0 && std::memcmp(marker,"NCA2",4)!=0 &&
       std::memcmp(marker,"NCA1",4)!=0 && std::memcmp(marker,"NCA0",4)!=0) {
        ++summary.encryptedOrUnknown;
        return "Cabecera no legible (normalmente cifrada); codigo NO accesible.";
    }
    ++summary.readableNca;
    const auto type=header[0x205];
    if(type==0)++summary.program;
    if(type==1)++summary.meta;
    if(type==2)++summary.control;
    std::ostringstream o;
    o<<"Cabecera "<<std::string(reinterpret_cast<const char*>(marker),4)
     <<" visible; tipo "<<ncaType(type)
     <<", tamano declarado "<<switchfile::read64(header.data()+0x208)
     <<" B, ID programa 0x"<<std::hex<<std::uppercase
     <<switchfile::read64(header.data()+0x210)<<std::dec
     <<". NO verifica integridad ni descifra secciones.";
    return o.str();
}
inline std::string reportNso(int fd,const Entry& e) {
    std::array<unsigned char,nso::kHeaderSize> header{};
    if(e.bytes<header.size() || !read(fd,header.data(),header.size(),e.absolute))
        return "Cabecera NSO incompleta.";
    const auto info=nso::inspect(header.data(),header.size(),e.bytes);
    if(!info.valid)return "NSO no validado: "+info.reason;
    std::ostringstream s;
    s<<"NSO0 ARM64 con .text="<<sizes(info.sections[0].memSize)
     <<", .rodata="<<sizes(info.sections[1].memSize)
     <<", .data="<<sizes(info.sections[2].memSize)
     <<", BSS="<<sizes(info.bssSize);
    s<<". Compresion text="<<(info.sections[0].compressed?"si":"no")
     <<"; solo CANDIDATO para recompilacion, aun no comprobado.";
    return s.str();
}
inline std::string reportNro(int fd,const Entry& e) {
    if(e.bytes>kMaxModuleRead)return "NRO demasiado grande para analisis completo (>16 MiB).";
    if(e.bytes<0x80)return "NRO demasiado pequeno.";
    std::vector<uint8_t> file(static_cast<size_t>(e.bytes));
    if(!read(fd,file.data(),file.size(),e.absolute))return "NRO inaccesible.";
    const auto res=nroinspect::inspect(file.data(),file.size());
    if(!res.ok)return "NRO no validado: "+res.error;
    std::ostringstream o;
    o<<"NRO0 HOMEbrew ARM64: .text="<<sizes(res.lengths[0])
     <<", .rodata="<<sizes(res.lengths[1])
     <<", .data="<<sizes(res.lengths[2])
     <<"; imports="<<(res.mod.ok?std::to_string(res.mod.importsFound):"sin datos")
     <<". Puede servir para probar el runtime, NO es el codigo de Zelda.";
    return o.str();
}
inline void classify(int fd,const Entry& e,Summary& summary,std::ostringstream& output,
                     bool visible) {
    if(summary.payloadBytes>UINT64_MAX-e.bytes)summary.payloadBytes=UINT64_MAX;
    else summary.payloadBytes+=e.bytes;
    if(visible)output<<"  "<<e.name<<" — "<<sizes(e.bytes)<<"\n";
    std::string info;
    if(suffix(e.name,".nca"))info=reportNca(fd,e,summary);
    else if(suffix(e.name,".tik")){++summary.tickets;info="Ticket; NO se extraen ni muestran claves.";}
    else if(suffix(e.name,".cert")){++summary.certificates;info="Certificado (no autentificado).";}
    else if(suffix(e.name,".nso")){++summary.executables;info=reportNso(fd,e);}
    else if(suffix(e.name,".nro")){++summary.executables;info=reportNro(fd,e);}
    else if(suffix(e.name,".ncz")){++summary.other;info="NCZ comprimido: aun no admitido.";}
    else {++summary.other;}
    if(visible && !info.empty())output<<"    "<<info<<"\n";
}
inline void listDirectory(int fd,const Directory& d,Summary& sum,std::ostringstream& out,
                          const std::string& tag) {
    out<<tag<<": "<<d.entries.size()<<" archivos\n";
    for(size_t i=0;i<d.entries.size();i++) {
        // Always summarize every entry but only read a tiny header and print a bounded list.
        classify(fd,d.entries[i],sum,out,i<kListedEntries);
    }
    if(d.entries.size()>kListedEntries)
        out<<"  ... otros "<<d.entries.size()-kListedEntries<<" archivos no mostrados\n";
}

inline std::string summarize(Summary sum) {
    std::ostringstream s;
    s<<"\nRESUMEN DE CONTENIDO\n"
     <<"NCA: "<<sum.nca<<" (cabecera legible "<<sum.readableNca
     <<", sin cabecera legible "<<sum.encryptedOrUnknown<<")\n"
     <<"  Program: "<<sum.program<<" | Meta: "<<sum.meta
     <<" | Control: "<<sum.control<<" (solo si cabecera legible)\n"
     <<"NSO/NRO directos: "<<sum.executables
     <<" | Tickets: "<<sum.tickets<<" | Certificados: "<<sum.certificates
     <<" | Otros: "<<sum.other<<"\n";
    if(sum.executables)
        s<<"CODIGO: hay ejecutables visibles; comprobar instrucciones e imports en su analizador.\n";
    else if(sum.program)
        s<<"CODIGO: NCA Program identificado; no equivale a tener un NSO extraido.\n";
    else
        s<<"CODIGO: no hay ejecutable NSO accesible en esta vista; se necesita ExeFS autorizado.\n";
    s<<"El titulo y la version comercial NO se deducen de nombres/cabeceras sin metadatos verificados.\n";
    return s.str();
}

inline std::string inspect(int fd) {
    struct stat st{};
    if(fd<0 || fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0)
        return error("Selecciona un archivo local, con tamano conocido y acceso aleatorio.");
    const uint64_t fileSize=static_cast<uint64_t>(st.st_size);
    if(fileSize<16)return error("Archivo demasiado pequeno.");
    std::array<unsigned char,0x200> head{};
    if(!read(fd,head.data(),size_t(std::min<uint64_t>(head.size(),fileSize)),0))
        return error("No se pudo leer la cabecera.");

    std::ostringstream out;
    out<<"EXPLORADOR PROFUNDO DE SWITCH (SOLO LECTURA)\n"
       <<"Tamano de archivo: "<<sizes(fileSize)<<"\n";
    Summary totals;

    if(std::memcmp(head.data(),"PFS0",4)==0) {
        out<<"Formato: NSP / PFS0\n\n";
        const auto dir=parseDirectory(fd,0,fileSize,false);
        if(!dir.ok)return error("NSP: "+dir.error);
        listDirectory(fd,dir,totals,out,"Indice del paquete");
        out<<summarize(totals);
    } else if(fileSize>=0x200 && std::memcmp(head.data()+0x100,"HEAD",4)==0) {
        out<<"Formato: XCI / imagen de cartucho\n";
        const uint64_t root=switchfile::read64(head.data()+0x130);
        const uint64_t rootSize=switchfile::read64(head.data()+0x138);
        out<<"Particion raiz declarada en 0x"<<std::hex<<root<<std::dec
           <<"; cabecera HFS0 indicada de "<<rootSize<<" bytes.\n";
        if(!within(root,16,fileSize) || rootSize<16 || !within(root,rootSize,fileSize))
            return error("XCI: datos de posicion de particion raiz invalidos.");
        const auto rootDir=parseDirectory(fd,root,fileSize-root,true);
        if(!rootDir.ok)return error("XCI: raiz HFS0 invalida: "+rootDir.error);
        if(rootSize<16+rootDir.entries.size()*64)
            return error("XCI: tamano de cabecera raiz incompatible con el indice.");
        out<<"Particiones raiz: "<<rootDir.entries.size()<<"\n";
        for(const auto& partition:rootDir.entries) {
            out<<"  "<<partition.name<<" — "<<sizes(partition.bytes)<<"\n";
            if(partition.name!="secure" && partition.name!="normal" &&
               partition.name!="update" && partition.name!="logo")continue;
            if(partition.bytes<16){
                out<<"    Particion vacia/incompleta.\n";continue;
            }
            const auto nested=parseDirectory(fd,partition.absolute,partition.bytes,true);
            if(!nested.ok){
                out<<"    HFS0 no legible: "<<nested.error<<"\n";
                continue;
            }
            listDirectory(fd,nested,totals,out,"    Contenido "+partition.name);
        }
        out<<summarize(totals);
    } else if(std::memcmp(head.data(),"NSO0",4)==0) {
        out<<"Formato: NSO0 extraido (candidato a codigo ARM64)\n";
        const Entry file{"main.nso",0,fileSize,0};
        out<<reportNso(fd,file)<<"\n";
        if(fileSize<=kMaxModuleRead) {
            std::vector<unsigned char> data(static_cast<size_t>(fileSize));
            if(read(fd,data.data(),data.size(),0)) {
                auto image=nsomemory::reconstruct(data.data(),data.size());
                if(image.ok) {
                    const auto mod=modinspect::parse(image);
                    out<<"Integridad secciones SHA-256 verificadas: "<<image.hashChecks<<"\n";
                    if(mod.ok) {
                        out<<"MOD0: "<<mod.dynamicEntries<<" etiquetas dinamicas; "
                           <<mod.importsFound<<" imports, "
                           <<mod.relaEntries<<" RELA, "<<mod.relEntries<<" REL, "
                           <<mod.relrTargets<<" destinos RELR.\n";
                        for(size_t i=0;i<mod.needed.size() && i<16;i++)
                            out<<"  Biblioteca requerida: "<<mod.needed[i]<<"\n";
                        for(size_t i=0;i<mod.importNames.size() && i<20;i++)
                            out<<"  Import: "<<mod.importNames[i]<<"\n";
                    } else out<<"MOD0 sin parsear: "<<mod.error<<"\n";
                } else out<<"Imagen no reconstruida: "<<image.error<<"\n";
            } else out<<"No se pudo leer el NSO para la fase MOD0.\n";
        } else out<<"Analisis MOD0 omitido por limite de RAM (16 MiB).\n";
        out<<"Codigo .text visible, pero aun falta desensamblar funciones, "
              "comprobar cobertura del recompilador y servicios Horizon.\n";
    } else if(fileSize>=0x80 && std::memcmp(head.data()+0x10,"NRO0",4)==0) {
        out<<"Formato: NRO0 homebrew de Switch\n";
        const Entry file{"homebrew.nro",0,fileSize,0};
        out<<reportNro(fd,file)<<"\n";
    } else if(fileSize>=0x224) {
        std::array<unsigned char,4> magic{};
        if(read(fd,magic.data(),magic.size(),0x200) &&
           (std::memcmp(magic.data(),"NCA3",4)==0 ||
            std::memcmp(magic.data(),"NCA2",4)==0)) {
            out<<"Formato: NCA con cabecera legible (NO significa recursos descifrados).\n";
            Summary n;
            out<<reportNca(fd,{"archivo.nca",0,fileSize,0},n)<<"\n";
        } else return error("No se reconocio NSP/XCI/NSO/NRO/NCA legible. "
                            "Los NCA cifrados sueltos no se pueden identificar solo por su magia.");
    } else return error("El archivo no coincide con un formato Switch reconocido.");

    out<<"\nQUE SE PUEDE UTILIZAR EN EL PORT\n"
       <<"* PFS0/HFS0: estructura e inventario de archivos, NO codigo ejecutable.\n"
       <<"* NCA cifrado: no permite ver instrucciones ni shaders.\n"
       <<"* NSO0 extraido y sin cifrar: es candidato para recompilacion ARM64.\n"
       <<"* NRO0: es candidato para pruebas de runtime/homebrew.\n"
       <<"* Para Zelda hacen falta ExeFS y RomFS de una copia legitima y compatible.\n"
       <<"\nPRIVACIDAD: archivo local, solo lectura; no envia ni extrae bytes.\n"
       <<"Este analisis no ejecuta Breath of the Wild ni comprueba que una ROM sea autentica.";
    auto message=out.str();
    if(message.size()>kMaxReportChars)message.resize(kMaxReportChars),
        message+="\n[Informe truncado por limite de salida.]";
    return message;
}
}  // namespace switchdeep
