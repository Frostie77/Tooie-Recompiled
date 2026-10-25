#include "android_touch_controls.h"
#include "touch_input.h"
#include <jni.h>
#include <SDL.h>
#include <SDL_system.h>
#include "recompui/recompui.h"
void tooie::touch::show_settings() {
    auto* env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if(!env) return;
    auto activity=static_cast<jobject>(SDL_AndroidGetActivity());
    auto type=activity&&!env->ExceptionCheck()?env->GetObjectClass(activity):nullptr;
    auto method=type&&!env->ExceptionCheck()?env->GetMethodID(type,"showTouchControlsSettings","()V"):nullptr;
    if(method&&!env->ExceptionCheck()) env->CallVoidMethod(activity,method);
    if(env->ExceptionCheck()) env->ExceptionClear();
    if(type) env->DeleteLocalRef(type);
    if(activity) env->DeleteLocalRef(activity);
}
extern "C" JNIEXPORT jlong JNICALL Java_io_github_banjotooierecompiled_TouchControlsView_nativeStatus(JNIEnv*,jclass,jboolean enabled) {
    auto& state=tooie::touch::input_state();state.set_enabled(enabled);
    // Focus-loss/pause calls must not wait on the renderer's UI-state mutex.
    if(enabled) state.set_gameplay(tooie::touch::gameplay_allowed());
    return static_cast<jlong>(state.status());
}
extern "C" JNIEXPORT jboolean JNICALL Java_io_github_banjotooierecompiled_TouchControlsView_nativeSubmit(JNIEnv*,jclass,jlong epoch,jint buttons,jfloat x,jfloat y) {
    return tooie::touch::input_state().publish(static_cast<uint64_t>(epoch),buttons,x,y);
}
extern "C" JNIEXPORT jlong JNICALL Java_io_github_banjotooierecompiled_TouchControlsView_nativeCancel(JNIEnv*,jclass) {
    return static_cast<jlong>(tooie::touch::input_state().cancel());
}
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_TouchControlsView_nativeMenu(JNIEnv*,jclass) {
    tooie::touch::input_state().cancel();if(!tooie::touch::gameplay_allowed()) return;
    // Only the UI queue receives this action; SDL's gameplay key state is unchanged.
    SDL_Event event{};event.type=SDL_KEYDOWN;event.key.state=SDL_PRESSED;
    event.key.keysym.sym=SDLK_ESCAPE;event.key.keysym.scancode=SDL_SCANCODE_ESCAPE;
    recompui::queue_event(event);event.type=SDL_KEYUP;event.key.state=SDL_RELEASED;recompui::queue_event(event);
}
