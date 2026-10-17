#include "nso_format.h"
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeInspectNso(JNIEnv* env, jclass, jint fd) {
    if (fd < 0) return env->NewStringUTF("Descriptor de archivo invalido.");
    struct stat st{};
    if (fstat(fd,&st) != 0) return env->NewStringUTF("No se pudo consultar el tamano del archivo.");
    if (!S_ISREG(st.st_mode) || st.st_size < 0) {
        return env->NewStringUTF("Este proveedor de archivos no permite lectura aleatoria.");
    }
    // pread does not change the offset and does not take ownership of the fd.
    std::array<unsigned char,nso::kHeaderSize> header{};
    ssize_t got = pread(fd, header.data(), header.size(), 0);
    if (got < 0) return env->NewStringUTF("No se pudo leer la cabecera.");
    nso::Info info=nso::inspect(header.data(),static_cast<std::size_t>(got),
                               static_cast<std::uint64_t>(st.st_size));
    const std::string message=nso::report(info);
    return env->NewStringUTF(message.c_str());
}
