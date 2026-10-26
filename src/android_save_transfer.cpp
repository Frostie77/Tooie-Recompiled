#include "android_save_transfer.h"
#if defined(__ANDROID__)
#include <jni.h>
#include <SDL.h>
#include <SDL_system.h>
#include "librecomp/game.hpp"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "elements/ui_label.h"

namespace {
void show_manager() {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env) return;
    auto activity = static_cast<jobject>(SDL_AndroidGetActivity());
    auto type = activity && !env->ExceptionCheck() ? env->GetObjectClass(activity) : nullptr;
    auto method = type && !env->ExceptionCheck() ? env->GetMethodID(type, "showSaveBackups", "()V") : nullptr;
    if (method && !env->ExceptionCheck()) env->CallVoidMethod(activity, method);
    bool failed = !method || env->ExceptionCheck();
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (type) env->DeleteLocalRef(type);
    if (activity) env->DeleteLocalRef(activity);
    if (failed) recompui::message_box("Save Management is unavailable. Please restart the app and try again.");
}
class SaveButton : public recompui::Button {
protected:
    void process_event(const recompui::Event& event) override {
        recompui::Button::process_event(event);
        if (event.type == recompui::EventType::Focus && std::get<recompui::EventFocus>(event.variant).active) scroll_into_view();
    }
public:
    using recompui::Button::Button;
};
class SaveSection : public recompui::Element {
    SaveButton* button;
protected:
    std::string_view get_type_name() override { return "TooieSaveBackups"; }
    void process_event(const recompui::Event& event) override {
        if (event.type != recompui::EventType::Update) return;
        button->set_enabled(!ultramodern::is_game_started());
        queue_update();
    }
public:
    SaveSection(recompui::ResourceId rid, recompui::Element* parent)
        : Element(rid, parent, recompui::Events(recompui::EventType::Update)) {
        set_display(recompui::Display::Flex); set_flex_direction(recompui::FlexDirection::Column);
        set_gap(12.0f); set_padding_top(24.0f); set_width(100.0f, recompui::Unit::Percent);
        auto context = recompui::get_current_context();
        context.create_element<recompui::Label>(this, "Save Management", recompui::LabelStyle::Large);
        context.create_element<recompui::Label>(this,
            "Back up or transfer all base-game save slots. (App settings, ROMs and mods are separate.)", recompui::LabelStyle::Small);
        button = context.create_element<SaveButton>(this, "Manage saves…", recompui::ButtonStyle::Secondary, recompui::ButtonSize::Medium);
        button->add_pressed_callback(show_manager);
        button->set_enabled(!ultramodern::is_game_started());
        context.create_element<recompui::Label>(this,
            "Use this feature at the Launcher Screen. If you are already in-game. Save and Exit then reopen the App.", recompui::LabelStyle::Small);
        queue_update();
    }
};
}
void tooie::saves::register_options() {
    recompui::config::append_tab_contents(recompui::config::general::id,
        [](recompui::ContextId context, recompui::Element* parent) { context.create_element<SaveSection>(parent); });
}
#else
void tooie::saves::register_options() {}
#endif
