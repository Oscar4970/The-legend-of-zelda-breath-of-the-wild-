#include "switch_deep_analyzer.h"
#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeAnalyzeSwitchDeep(JNIEnv* env,jclass,jint fd) {
    const std::string report=switchdeep::inspect(fd);
    return env->NewStringUTF(report.c_str());
}
