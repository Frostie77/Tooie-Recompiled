#include <jni.h>
#include <cassert>
#include "android_updates_jni.h"
static thread_local JNIEnv* current_env;
static thread_local jobject current_activity;
extern "C" void* SDL_AndroidGetJNIEnv() { return current_env; }
extern "C" void* SDL_AndroidGetActivity() { return current_activity ? current_env->NewLocalRef(current_activity) : nullptr; }
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_UpdateJniTest_exercise(JNIEnv* env, jclass, jobject activity, jint mode) {
    current_env = env; current_activity = activity;
    using namespace tooie::updates::android;
    if (mode == 0) {
        check(true); check(false);
        assert(state() == "{\"state\":\"available\"}");
        assert(can_show_notice()); open_release();
    } else {
        assert(state().empty()); assert(!env->ExceptionCheck());
        if (mode == 2) { check(true); open_release(); assert(!can_show_notice()); }
    }
    current_activity = nullptr; current_env = nullptr;
}
