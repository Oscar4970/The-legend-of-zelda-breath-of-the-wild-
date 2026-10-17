#include "synthetic_nso.h"
#include <jni.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

#if defined(__aarch64__) && defined(__ANDROID__)
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeRunSyntheticArm64(JNIEnv* env,jclass) {
#if !defined(__aarch64__) || !defined(__ANDROID__)
    return env->NewStringUTF("Solo compatible con Android ARM64.");
#else
    const auto bytes=synthetic::makeNso(); // Hardcoded, allowlisted test; never reads user ROMs
    const auto image=nsomemory::reconstruct(bytes.data(),bytes.size());
    if(!synthetic::allowed(image))
        return env->NewStringUTF("Prueba cancelada: código de prueba no autorizado.");

    long page=sysconf(_SC_PAGESIZE);
    if(page<4096 || page>1048576)
        return env->NewStringUTF("Prueba cancelada: tamaño de pagina inesperado.");

    // W^X: map writable+not executable, then revoke write and enable execute.
    // Never map PROT_WRITE|PROT_EXEC at the same time.
    void* executable=mmap(nullptr,static_cast<std::size_t>(page),
                          PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(executable==MAP_FAILED)
        return env->NewStringUTF("Android denego reservar memoria de prueba.");

    std::memcpy(executable,image.bytes.data(),synthetic::kAllowedText.size());
    auto* start=reinterpret_cast<char*>(executable);
    __builtin___clear_cache(start,start+synthetic::kAllowedText.size());
    if(mprotect(executable,static_cast<std::size_t>(page),PROT_READ|PROT_EXEC)!=0){
        const int err=errno;
        munmap(executable,static_cast<std::size_t>(page));
        std::ostringstream report;
        report<<"Android denego memoria ejecutable (mprotect errno "<<err
              <<"). Esta limitacion NO afecta tu celular.";
        const auto str=report.str();
        return env->NewStringUTF(str.c_str());
    }

    // This is a fixed, audited two-instruction AArch64 function with no syscalls
    // or branches; parameters and return value use the Android/AArch64 C ABI.
    using Function=std::uint32_t(*)(std::uint32_t,std::uint32_t);
    auto function=reinterpret_cast<Function>(executable);
    const auto result=function(19,23);
    munmap(executable,static_cast<std::size_t>(page));

    std::ostringstream report;
    report<<"NSO ficticio: cabecera y hashes validados ("<<image.hashChecks<<")\n"
          <<"LZ4 descomprimido: "<<image.sectionLengths[0]<<" bytes ARM64\n"
          <<"Codigo ARM64 ejecutado desde memoria RX: 19 + 23 = "<<result<<"\n"
          <<"Resultado: "<<(result==42?"PRUEBA CORRECTA":"PRUEBA FALLIDA")
          <<"\n\nNO es codigo de Zelda. No admite codigo externo, servicios de Switch ni NVN.";
    const auto message=report.str();
    return env->NewStringUTF(message.c_str());
#endif
}
