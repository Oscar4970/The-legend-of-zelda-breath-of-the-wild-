#include "switch_container_inspector.h"
#include <jni.h>
#include <string>
extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeInspectSwitchContainer(JNIEnv* env,jclass,jint fd) {
    std::string s=switchfile::inspect(fd);
    return env->NewStringUTF(s.c_str());
}
