#include "../src/touch_input.h"
#include "../src/android_touch_controls.h"
#include <jni.h>
#include <SDL.h>
#include <vector>
#include <cassert>
static JavaVM* vm;
static jobject activity;
static bool started=false,disabled=false,binding=false;
static int queries=0;
static std::vector<SDL_Event> events;
namespace ultramodern { bool is_game_started() { ++queries;return started; } }
namespace recompinput {
bool game_input_disabled() { return disabled; }
bool all_input_disabled() { return binding; }
namespace profiles { bool get_n64_input(int player,uint16_t* b,float* x,float* y) { *b=disabled?0:0x20;*x=disabled?0:.25f;*y=disabled?0:-.4f;return player==0; } }
}
namespace recompui { void queue_event(const SDL_Event& event) { events.push_back(event); } }
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* value,void*) { vm=value;return JNI_VERSION_1_6; }
extern "C" void* SDL_AndroidGetJNIEnv() { JNIEnv* env=nullptr;return vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)==JNI_OK?env:nullptr; }
extern "C" void* SDL_AndroidGetActivity() { auto env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());return activity?env->NewLocalRef(activity):nullptr; }
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_TouchNativeTest_init(JNIEnv* env,jclass,jobject object) {
    if(activity)env->DeleteGlobalRef(activity);activity=object?env->NewGlobalRef(object):nullptr;
}
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_TouchNativeTest_runtime(JNIEnv*,jclass,jboolean game,jboolean menu,jboolean bind) { started=game;disabled=menu;binding=bind;queries=0; }
extern "C" JNIEXPORT jint JNICALL Java_io_github_banjotooierecompiled_TouchNativeTest_queries(JNIEnv*,jclass) { return queries; }
extern "C" JNIEXPORT jint JNICALL Java_io_github_banjotooierecompiled_TouchNativeTest_menuEvents(JNIEnv*,jclass) {
    if(!events.empty())assert(events.size()==2&&events[0].type==SDL_KEYDOWN&&events[1].type==SDL_KEYUP&&events[0].key.keysym.scancode==SDL_SCANCODE_ESCAPE);
    return events.size();
}
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_TouchNativeTest_settings(JNIEnv*,jclass) { tooie::touch::show_settings(); }
extern "C" JNIEXPORT jfloatArray JNICALL Java_io_github_banjotooierecompiled_TouchNativeTest_sample(JNIEnv* env,jclass,jint player) {
    uint16_t b;float x,y;bool connected=tooie::touch::read_n64_input(player,&b,&x,&y);jfloat values[]={float(b),x,y,connected?1.f:0.f};
    auto result=env->NewFloatArray(4);env->SetFloatArrayRegion(result,0,4,values);return result;
}
