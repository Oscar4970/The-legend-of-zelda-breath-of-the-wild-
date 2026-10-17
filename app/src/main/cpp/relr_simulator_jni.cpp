#include "relr_sandbox.h"
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <vector>

// IMPORTANT: the selected file is READ, never run. No mmap(PROT_EXEC).
extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeSimulateRelr(JNIEnv* env,jclass,jint fd) {
    if(fd<0)return env->NewStringUTF("Descriptor de archivo invalido.");
    struct stat st{};
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0)
        return env->NewStringUTF("Se necesita un archivo local NRO de tamano conocido.");
    if(static_cast<std::uint64_t>(st.st_size)>nroinspect::kMaxInput)
        return env->NewStringUTF("Archivo demasiado grande (maximo 16 MiB).");
    const std::size_t length=static_cast<std::size_t>(st.st_size);
    std::vector<std::uint8_t> file(length);
    std::size_t at=0;
    while(at<length) {
        const ssize_t bytes=pread(fd,file.data()+at,length-at,static_cast<off_t>(at));
        if(bytes<=0)return env->NewStringUTF("No fue posible leer el archivo.");
        at+=static_cast<std::size_t>(bytes);
    }
    const auto info=nroinspect::inspect(file.data(),file.size());
    if(!info.ok || !info.mod.ok) {
        const auto report=nroinspect::report(info);
        return env->NewStringUTF(report.c_str());
    }
    const auto image=relrsandbox::copyImage(file.data(),file.size(),info);
    const auto trial=relrsandbox::simulate(image,info.mod);
    const auto report=relrsandbox::report(trial);
    return env->NewStringUTF(report.c_str());
}
