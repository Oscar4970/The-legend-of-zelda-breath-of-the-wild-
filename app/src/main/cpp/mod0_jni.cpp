#include "mod0_inspector.h"
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <vector>
#include <string>

// Read only metadata from a locally selected, decrypted NSO0.
// No network, no write permissions, no code execution, no bypass of encryption.
extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeInspectMod0(JNIEnv* env,jclass,jint fd) {
    if(fd<0)return env->NewStringUTF("Descriptor invalido.");
    struct stat st{};
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0) {
        return env->NewStringUTF("Se requiere un archivo local con tamano conocido.");
    }
    constexpr std::uint64_t limit=16ULL*1024*1024;
    if(static_cast<std::uint64_t>(st.st_size)>limit) {
        return env->NewStringUTF("Archivo de mas de 16 MiB: rechazado por seguridad.");
    }
    std::vector<std::uint8_t> file(static_cast<std::size_t>(st.st_size));
    std::size_t offset=0;
    while(offset<file.size()) {
        const auto n=pread(fd,file.data()+offset,file.size()-offset,static_cast<off_t>(offset));
        if(n<=0)return env->NewStringUTF("No fue posible leer el NSO completo.");
        offset+=static_cast<std::size_t>(n);
    }
    const auto image=nsomemory::reconstruct(file.data(),file.size());
    if(!image.ok) {
        const auto error="No se pudo reconstruir NSO: "+image.error;
        return env->NewStringUTF(error.c_str());
    }
    const auto metadata=modinspect::parse(image);
    const auto report=modinspect::report(metadata);
    return env->NewStringUTF(report.c_str());
}
