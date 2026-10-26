#include "../src/android_driver_settings.h"
#include "../src/android_vulkan_driver_policy.h"
#include <jni.h>
#include <cassert>
#include <limits>
static JavaVM* vm;
static jobject peer;
static bool noEnv=false;
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* value,void*) {vm=value;return JNI_VERSION_1_6;}
extern "C" void* SDL_AndroidGetJNIEnv() {JNIEnv* env=nullptr;return !noEnv&&vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)==JNI_OK?env:nullptr;}
extern "C" void* SDL_AndroidGetActivity() {auto* env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());return peer&&env?env->NewLocalRef(peer):nullptr;}
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_DriverJniTest_init(JNIEnv* env,jclass,jobject value) {
    if(peer)env->DeleteGlobalRef(peer);peer=value?env->NewGlobalRef(value):nullptr;
}
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_DriverJniTest_probe(JNIEnv*,jclass,jint mode) {
    using namespace tooie::android::gpu;
    noEnv=mode==3;
    Selection selected;selected.mode=mode==0?Mode::System:mode==1?Mode::Custom:Mode::Invalid;
    selected.packageSha256="zip";selected.librarySha256="library";if(mode==2)selected.error="injected-rejection";
    recordSelection(selected);recordBackend(true,18,"Adreno 640","Mesa Turnip","measured driver",std::numeric_limits<unsigned>::max(),true);
    recordMenuReady();show_settings(2);auto values=settings_state();
    if(!noEnv && peer && values[0]!="")for(int i=0;i<4;i++)assert(values[i]=="state-"+std::to_string(i));
    noEnv=false;
}
