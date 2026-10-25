#include "android_touch_controls.h"
#include "touch_input.h"
#include <jni.h>
#include <SDL.h>
#include <SDL_system.h>
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "elements/ui_label.h"

namespace {
void show_settings() {
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
class TouchPage : public recompui::Element {
protected:
    std::string_view get_type_name() override { return "TooieTouchControls"; }
public:
    TouchPage(recompui::ResourceId rid,recompui::Element* parent):Element(rid,parent,0) {
        set_display(recompui::Display::Flex);set_flex_direction(recompui::FlexDirection::Column);
        set_gap(20.0f);set_padding(24.0f);set_width(100.0f,recompui::Unit::Percent);
        auto context=recompui::get_current_context();
        context.create_element<recompui::Label>(this,"On-screen controls for phone play",recompui::LabelStyle::Normal);
        context.create_element<recompui::Label>(this,"Choose Auto, On or Off. Adjust button and joystick sizes, opacity and positions.",recompui::LabelStyle::Normal);
        auto button=context.create_element<recompui::Button>(this,"Configure touch controls",recompui::ButtonStyle::Primary,recompui::ButtonSize::Medium);
        button->add_pressed_callback(show_settings);
    }
};
}
void tooie::touch::register_options() {
    recompui::config::create_tab("Touch","touch",[](recompui::ContextId context,recompui::Element* parent) {
        context.create_element<TouchPage>(parent);
    });
}
extern "C" JNIEXPORT jlong JNICALL Java_io_github_banjotooierecompiled_TouchControlsView_nativeStatus(JNIEnv*,jclass,jboolean enabled) {
    auto& state=tooie::touch::input_state();
    state.set_enabled(enabled);
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
    tooie::touch::input_state().cancel();
    if(!tooie::touch::gameplay_allowed()) return;
    // Only the UI queue receives this menu action; SDL's gameplay key state is unchanged.
    SDL_Event event{};event.type=SDL_KEYDOWN;event.key.state=SDL_PRESSED;
    event.key.keysym.sym=SDLK_ESCAPE;event.key.keysym.scancode=SDL_SCANCODE_ESCAPE;
    recompui::queue_event(event);
    event.type=SDL_KEYUP;event.key.state=SDL_RELEASED;recompui::queue_event(event);
}
