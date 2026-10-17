#include "nro_runtime_audit.h"
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <vector>

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeAuditNroRuntime(JNIEnv* env,jclass,jint fd) {
    if(fd<0)return env->NewStringUTF("Descriptor de archivo invalido.");
    struct stat st{};
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0){
        return env->NewStringUTF("Se requiere un archivo local con tamano conocido.");
    }
    if(static_cast<std::uint64_t>(st.st_size)>nroinspect::kMaxInput) {
        return env->NewStringUTF("Archivo NRO superior al limite seguro de 16 MiB.");
    }
    const auto size=static_cast<std::size_t>(st.st_size);
    std::vector<std::uint8_t> file(size);
    std::size_t cursor=0;
    while(cursor<size) {
        const auto got=pread(fd,file.data()+cursor,size-cursor,static_cast<off_t>(cursor));
        if(got<=0)return env->NewStringUTF("Fallo leyendo el archivo local.");
        cursor+=static_cast<std::size_t>(got);
    }
    const auto report=runtimeaudit::examine(file.data(),file.size());
    const auto text=runtimeaudit::display(report);
    return env->NewStringUTF(text.c_str());
}
