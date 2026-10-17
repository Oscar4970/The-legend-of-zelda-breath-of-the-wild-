#include "nso_memory.h"
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeReconstructNso(JNIEnv* env,jclass,jint fd){
    if(fd<0)return env->NewStringUTF("Descriptor invalido.");
    struct stat st{};
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0)
        return env->NewStringUTF("Se requiere un archivo local con tamano conocido.");
    constexpr std::uint64_t kMaxInput=16ULL*1024*1024;
    if(static_cast<std::uint64_t>(st.st_size)>kMaxInput)
        return env->NewStringUTF("Archivo mayor a 16 MiB: omitido por seguridad.");
    const auto size=static_cast<std::size_t>(st.st_size);
    std::vector<std::uint8_t> file(size);
    std::size_t got=0;
    while(got<size){
        ssize_t readBytes=pread(fd,file.data()+got,size-got,static_cast<off_t>(got));
        if(readBytes<=0)return env->NewStringUTF("No fue posible leer el archivo completo.");
        got+=static_cast<std::size_t>(readBytes);
    }
    const auto memory=nsomemory::reconstruct(file.data(),file.size());
    if(!memory.ok){
        auto error="No fue posible reconstruir NSO: "+memory.error;
        return env->NewStringUTF(error.c_str());
    }
    std::ostringstream report;
    report<<"Imagen de memoria reconstruida: "<<memory.bytes.size()
          <<" bytes\nBase virtual: "<<memory.base
          <<"\nSecciones: "<<memory.sectionLengths[0]<<", "
          <<memory.sectionLengths[1]<<", "<<memory.sectionLengths[2]
          <<"\nBSS: "<<memory.bssLength<<" bytes (inicializados a cero)"
          <<"\nHashes SHA-256 verificados: "<<memory.hashChecks
          <<"\n\nIMPORTANTE: memoria aislada, no ejecuta instrucciones ARM64, "
          <<"no verifica firmas digitales ni implementa servicios Switch o NVN.";
    return env->NewStringUTF(report.str().c_str());
}
