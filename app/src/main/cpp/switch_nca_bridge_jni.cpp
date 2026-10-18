#include "switch_nca_bridge.h"
#include <jni.h>
#include <stdexcept>

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_example_botwlab_MainActivity_nativeNcaHeaderSamples(JNIEnv* env, jclass, jint fd) {
    try {
        auto samples = ncabridge::sampleHeaders(fd);
        jclass byteArrayClass = env->FindClass("[B");
        if (!byteArrayClass) return nullptr;
        auto array = env->NewObjectArray(static_cast<jsize>(samples.size()), byteArrayClass, nullptr);
        if (!array) return nullptr;
        for (size_t i = 0; i < samples.size(); ++i) {
            auto bytes = env->NewByteArray(static_cast<jsize>(ncabridge::kHeaderBytes));
            if (!bytes) return nullptr;
            env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(ncabridge::kHeaderBytes),
                                    reinterpret_cast<const jbyte*>(samples[i].data()));
            env->SetObjectArrayElement(array, static_cast<jsize>(i), bytes);
            env->DeleteLocalRef(bytes);
            if (env->ExceptionCheck()) return nullptr;
        }
        return array;
    } catch (const std::exception&) {
        jclass error = env->FindClass("java/lang/IllegalArgumentException");
        if (error) env->ThrowNew(error, "NSP no válido para inspección NCA local.");
        return nullptr;
    }
}
