#include "android_updates_jni.h"
#if defined(__ANDROID__)
#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>

namespace tooie::updates::android {
namespace {
struct Activity {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject object = env ? static_cast<jobject>(SDL_AndroidGetActivity()) : nullptr;
    jclass type = object ? env->GetObjectClass(object) : nullptr;
    ~Activity() {
        if (!env) return;
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (type) env->DeleteLocalRef(type);
        if (object) env->DeleteLocalRef(object);
    }
    jmethodID method(const char* name, const char* signature) {
        return type && !env->ExceptionCheck() ? env->GetMethodID(type, name, signature) : nullptr;
    }
};
}
void check(bool automatic) {
    Activity activity;
    if (auto method = activity.method("requestUpdateCheck", "(Z)V"))
        activity.env->CallVoidMethod(activity.object, method, jboolean(automatic));
}
std::string state() {
    Activity activity;
    auto method = activity.method("getUpdateState", "()Ljava/lang/String;");
    if (!method) return {};
    auto* env = activity.env;
    auto result = static_cast<jstring>(env->CallObjectMethod(activity.object, method));
    if (env->ExceptionCheck()) { if (result) env->DeleteLocalRef(result); return {}; }
    if (!result) return {};
    const char* text = env->GetStringUTFChars(result, nullptr);
    std::string output = text ? text : "";
    if (text) env->ReleaseStringUTFChars(result, text);
    env->DeleteLocalRef(result);
    return output;
}
bool can_show_notice() {
    Activity activity;
    if (auto method = activity.method("canShowUpdateNotice", "()Z")) {
        bool value = activity.env->CallBooleanMethod(activity.object, method);
        return !activity.env->ExceptionCheck() && value;
    }
    return false;
}
void open_release() {
    Activity activity;
    if (auto method = activity.method("openUpdateRelease", "()V"))
        activity.env->CallVoidMethod(activity.object, method);
}
}
#endif
