// Bounded PFS0 (.NSP) and HEAD (.XCI) structural check; never reads file payloads.
#pragma once
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

namespace switchfile {
inline uint32_t read32(const unsigned char* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
inline uint64_t read64(const unsigned char* p) { return uint64_t(read32(p)) | uint64_t(read32(p+4))<<32; }
inline bool readAt(int fd, void* dest, size_t amount, uint64_t offset) {
    if (fd<0 || offset>uint64_t(INT64_MAX) || amount>uint64_t(INT64_MAX)-offset) return false;
    auto* p=static_cast<unsigned char*>(dest);
    for(size_t n=0;n<amount;) {
        ssize_t r=pread(fd,p+n,amount-n,off_t(offset+n));
        if(r<0 && errno==EINTR) continue;
        if(r<=0) return false;
        n+=size_t(r);
    }
    return true;
}
inline std::string fail(const std::string& reason) {
    return "NSP/XCI — NO VALIDADO\nMotivo: "+reason+"\nNingun archivo fue modificado.";
}
inline std::string inspect(int fd) {
    struct stat st{};
    if(fd<0 || fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<16)
        return fail("Se necesita un archivo local de tamano conocido y acceso aleatorio.");
    uint64_t size=uint64_t(st.st_size);
    unsigned char head[16]{};
    if(!readAt(fd,head,sizeof head,0))return fail("Error leyendo cabecera.");
    std::ostringstream o;
    o<<"INSPECCION LOCAL (sin extraer ni descifrar)\nTamano: "<<size<<" bytes\n";
    if(memcmp(head,"PFS0",4)!=0) {
        unsigned char h[4]{};
        if(size>=0x104 && readAt(fd,h,sizeof h,0x100) && memcmp(h,"HEAD",4)==0) {
            o<<"Formato: XCI, firma HEAD reconocida.\n"
             <<"NO se verificaron las particiones ni el juego.\nNo se ejecuto Zelda.";
            return o.str();
        }
        return fail("No se encontro firma NSP PFS0 ni firma XCI HEAD.");
    }
    const uint32_t count=read32(head+4), length=read32(head+8);
    if(count>20000 || length>8u*1024u*1024u)return fail("Indices desproporcionados.");
    uint64_t entriesBytes=uint64_t(count)*24;
    uint64_t dataStart=16+entriesBytes+length;
    if(dataStart>size)return fail("PFS0 truncado.");
    std::vector<unsigned char> entries(size_t(entriesBytes)),names(length);
    if(!readAt(fd,entries.data(),entries.size(),16) ||
       !readAt(fd,names.data(),names.size(),16+entriesBytes))
        return fail("No se pudo leer el indice PFS0.");
    uint32_t nca=0,tik=0,cert=0,other=0;
    uint64_t sum=0;
    for(uint32_t i=0;i<count;i++) {
        const auto* e=entries.data()+uint64_t(i)*24;
        uint64_t off=read64(e), len=read64(e+8);
        uint32_t nameOff=read32(e+16);
        if(nameOff>=length)return fail("Nombre fuera de limites.");
        const char* start=reinterpret_cast<const char*>(names.data()+nameOff);
        const char* end=static_cast<const char*>(memchr(start,0,length-nameOff));
        if(!end || end==start || end-start>255)return fail("Nombre de indice incorrecto.");
        std::string name(start,end);
        for(unsigned char c:name) if(c<32 || c>126 || c=='/' || c=='\\')
            return fail("Nombre de indice inseguro.");
        if(off>size-dataStart || len>size-dataStart-off)
            return fail("Entrada PFS0 fuera del contenedor.");
        if(UINT64_MAX-sum<len)return fail("Overflow de tamanos.");
        sum+=len;
        std::transform(name.begin(),name.end(),name.begin(),
            [](unsigned char c){return char(std::tolower(c));});
        if(name.size()>=4 && name.compare(name.size()-4,4,".nca")==0)nca++;
        else if(name.size()>=4 && name.compare(name.size()-4,4,".tik")==0)tik++;
        else if(name.size()>=5 && name.compare(name.size()-5,5,".cert")==0)cert++;
        else other++;
    }
    o<<"Formato: NSP (PFS0)\n"
      <<"Cabecera e indice estructuralmente coherentes.\n"
      <<"Entradas: "<<count<<"\nNCA: "<<nca<<" | TIK: "<<tik
      <<" | CERT: "<<cert<<" | otros: "<<other<<"\n"
      <<"Tamano interno declarado: "<<sum<<" bytes\n\n"
      <<"NO confirma que el juego sea Zelda ni verifica su version/integridad.\n"
      <<"NO se leyeron ni extrajeron los archivos NCA.\n"
      <<"NO se subio el archivo a internet.\n"
      <<"NO ejecuta el juego.";
    return o.str();
}
} // namespace switchfile
