// Android JNI glue for the NSP virtual filesystem mount.
// Keeps its own duplicated descriptor after Android's picker FD closes.
#include "switch_nsp_vfs.h"
#include <jni.h>
#include <vector>
#include <cstdint>
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

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_example_botwlab_SwitchMountedExeFsBridge_nativeEntryNames(JNIEnv* env, jclass) {
    try {
        auto mount=nspvfs::currentMount();
        if (!mount) return nullptr;
        const auto names=mount->entryNames();
        if (names.size()>20000) return nullptr;
        jclass cls=env->FindClass("java/lang/String");
        if (!cls) return nullptr;
        jobjectArray result=env->NewObjectArray(static_cast<jsize>(names.size()),cls,nullptr);
        if (!result) return nullptr;
        for (jsize i=0;i<static_cast<jsize>(names.size());++i) {
            jstring name=env->NewStringUTF(names[static_cast<size_t>(i)].c_str());
            if (!name) return nullptr;
            env->SetObjectArrayElement(result,i,name);
            env->DeleteLocalRef(name);
            if (env->ExceptionCheck()) return nullptr;
        }
        return result;
    } catch (...) { return nullptr; }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_example_botwlab_SwitchMountedExeFsBridge_nativeEntrySize(
    JNIEnv* env,jclass,jstring name) {
    try {
        auto mount=nspvfs::currentMount();
        if (!mount || !name) return -1;
        const char* text=env->GetStringUTFChars(name,nullptr);
        if (!text) return -1;
        std::string value(text);
        env->ReleaseStringUTFChars(name,text);
        const uint64_t bytes=mount->sizeOf(value);
        return bytes<=static_cast<uint64_t>(INT64_MAX)?static_cast<jlong>(bytes):-1;
    } catch (...) { return -1; }
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_org_example_botwlab_SwitchMountedExeFsBridge_nativeReadEntry(
    JNIEnv* env,jclass,jstring name,jlong offset,jint length) {
    if (!name || offset<0 || length<0 || length>65536) return nullptr;
    try {
        auto mount=nspvfs::currentMount();
        if (!mount) return nullptr;
        const char* text=env->GetStringUTFChars(name,nullptr);
        if (!text) return nullptr;
        std::string path(text);
        env->ReleaseStringUTFChars(name,text);
        std::vector<unsigned char> bytes(static_cast<size_t>(length));
        if (length!=0) mount->read(path,static_cast<uint64_t>(offset),bytes.data(),bytes.size());
        jbyteArray result=env->NewByteArray(length);
        if (result && length)
            env->SetByteArrayRegion(result,0,length,reinterpret_cast<const jbyte*>(bytes.data()));
        std::fill(bytes.begin(),bytes.end(),0);
        return result;
    } catch (...) { return nullptr; }
}
