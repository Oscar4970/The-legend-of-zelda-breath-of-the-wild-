// Android JNI glue for the NSP virtual filesystem mount.
// Keeps its own duplicated descriptor after Android's picker FD closes.
#include "switch_nsp_vfs.h"
#include <jni.h>
#include <memory>
#include <mutex>
#include <string>

namespace {
std::mutex mountMutex;
std::shared_ptr<nspvfs::Mount> activeMount;

jstring asJavaString(JNIEnv* env, const std::string& text) {
    return env->NewStringUTF(text.c_str());
}
}

// These accessors are the native runtime integration point for future
// ExeFS/RomFS virtualization, without requiring a second NSP copy.
namespace nspvfs {
std::shared_ptr<const Mount> currentMount() {
    std::lock_guard<std::mutex> lock(mountMutex);
    return activeMount;
}
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeMountNspVfs(JNIEnv* env, jclass, jint fd) {
    try {
        auto candidate=std::make_shared<nspvfs::Mount>(fd);
        std::string report=candidate->diagnostic();
        {
            std::lock_guard<std::mutex> lock(mountMutex);
            activeMount=std::move(candidate);
        }
        return asJavaString(env, report);
    } catch (const std::exception& ex) {
        return asJavaString(env,
            std::string("VFS: no se pudo montar el NSP (")+
            ex.what()+"). El juego no se modifico.");
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeProbeNspVfs(JNIEnv* env, jclass) {
    try {
        auto mount=nspvfs::currentMount();
        if (!mount) return asJavaString(env,"VFS: ningun NSP esta montado.");
        return asJavaString(env,mount->selfTest());
    } catch (const std::exception&) {
        return asJavaString(env,"VFS: fallo una lectura aleatoria local.");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_botwlab_MainActivity_nativeUnmountNspVfs(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(mountMutex);
    activeMount.reset();
}
