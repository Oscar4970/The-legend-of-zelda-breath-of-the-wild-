#include "nro_inspector.h"
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <vector>
#include <string>

// Parse user-chosen, independently obtained homebrew NRO without executing it.
// The file stays local and its bytes never enter executable pages.
extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeInspectNro(JNIEnv* env,jclass,jint fd){
    if(fd<0)return env->NewStringUTF("Descriptor invalido.");
    struct stat statbuf{};
    if(fstat(fd,&statbuf)!=0 || !S_ISREG(statbuf.st_mode) || statbuf.st_size<0){
        return env->NewStringUTF("Necesitas un archivo local con tamano conocido.");
    }
    if(static_cast<std::uint64_t>(statbuf.st_size)>nroinspect::kMaxInput){
        return env->NewStringUTF("NRO mayor que 16 MiB; omitido por seguridad.");
    }
    const auto count=static_cast<std::size_t>(statbuf.st_size);
    std::vector<std::uint8_t> bytes(count);
    std::size_t offset=0;
    while(offset<count){
        const auto got=pread(fd,bytes.data()+offset,count-offset,static_cast<off_t>(offset));
        if(got<=0)return env->NewStringUTF("No se pudo leer el archivo NRO completo.");
        offset+=static_cast<std::size_t>(got);
    }
    const auto result=nroinspect::inspect(bytes.data(),bytes.size());
    const auto message=nroinspect::report(result);
    return env->NewStringUTF(message.c_str());
}
