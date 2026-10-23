#include "android_updates.h"

#if defined(__ANDROID__)
#include "android_update_notice.h"
#include "android_updates_jni.h"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "elements/ui_label.h"
#include "elements/ui_toggle.h"
#include "librecomp/config.hpp"
#include "ultramodern/ultramodern.hpp"
#include <json/json.hpp>

namespace tooie::updates {
namespace {
constexpr const char* automatic_option = "automatic_update_checks";
NoticePolicy notice_policy;
uint64_t next_launcher_poll = 0;

bool automatic_enabled() {
    return std::get<bool>(recompui::config::get_general_config().get_option_value(automatic_option));
}
struct State {
    std::string status, message, installed, available;
    bool automatic = false;
};
State read_state() {
    State state{"unavailable", "Update checking is unavailable. You can keep playing.", "Unknown", "", false};
    auto value = nlohmann::json::parse(android::state(), nullptr, false);
    if (!value.is_object()) return state;
    if (value.contains("state") && value["state"].is_string()) state.status = value["state"];
    if (value.contains("message") && value["message"].is_string()) state.message = value["message"];
    if (value.contains("installed_version") && value["installed_version"].is_string()) state.installed = value["installed_version"];
    if (value.contains("available_version") && value["available_version"].is_string()) state.available = value["available_version"];
    if (value.contains("automatic") && value["automatic"].is_boolean()) state.automatic = value["automatic"];
    return state;
}

class UpdatePage : public recompui::Element {
    recompui::Label *installed_label, *status_label;
    recompui::Button *check_button, *view_button;
    recompui::Toggle* automatic_toggle;
    uint64_t next_poll = 0;
    std::string last_message;
protected:
    std::string_view get_type_name() override { return "TooieUpdates"; }
    void process_event(const recompui::Event& event) override {
        if (event.type != recompui::EventType::Update) return;
        if (SDL_GetTicks64() >= next_poll) {
            next_poll = SDL_GetTicks64() + 500;
            refresh();
        }
        queue_update();
    }
    void refresh() {
        State state = read_state();
        if (last_message != state.message + state.installed) {
            installed_label->set_text("Installed version: " + state.installed);
            status_label->set_text(state.message);
            last_message = state.message + state.installed;
        }
        bool supported = state.status != "unsupported";
        check_button->set_enabled(supported && state.status != "checking");
        automatic_toggle->set_enabled(supported);
        view_button->set_enabled(state.status == "available");
    }
public:
    UpdatePage(recompui::ResourceId rid, recompui::Element* parent)
        : Element(rid, parent, recompui::Events(recompui::EventType::Update)) {
        set_display(recompui::Display::Flex);
        set_flex_direction(recompui::FlexDirection::Column);
        set_gap(20.0f);
        set_padding(24.0f);
        set_width(100.0f, recompui::Unit::Percent);
        auto context = recompui::get_current_context();
        installed_label = context.create_element<recompui::Label>(this, "Installed version", recompui::LabelStyle::Normal);
        status_label = context.create_element<recompui::Label>(this, "Ready to check for updates.", recompui::LabelStyle::Normal);
        auto* row = context.create_element<recompui::Element>(this, 0, "div");
        row->set_display(recompui::Display::Flex);
        row->set_align_items(recompui::AlignItems::Center);
        row->set_gap(16.0f);
        automatic_toggle = context.create_element<recompui::Toggle>(row);
        automatic_toggle->set_checked(automatic_enabled());
        automatic_toggle->add_checked_callback([](bool enabled) {
            auto& config = recompui::config::get_general_config();
            config.update_option_value(automatic_option, enabled);
            config.save_config();
            if (enabled) android::check(true);
        });
        context.create_element<recompui::Label>(row, "Check automatically at startup", recompui::LabelStyle::Normal);
        check_button = context.create_element<recompui::Button>(this, "Check for Updates", recompui::ButtonStyle::Primary, recompui::ButtonSize::Medium);
        check_button->add_pressed_callback([this] { android::check(false); refresh(); });
        view_button = context.create_element<recompui::Button>(this, "View Release", recompui::ButtonStyle::Secondary, recompui::ButtonSize::Medium);
        view_button->add_pressed_callback([] { android::open_release(); });
        context.create_element<recompui::Label>(this,
            "Download the APK from the release's Assets and install it as an update. Keep the app installed to retain your progress.",
            recompui::LabelStyle::Small);
        refresh();
        queue_update();
    }
};
}

void register_options(recomp::config::Config& general) {
    // Store alongside the existing general preferences; render the control in
    // Updates so the approved five-row launcher layout stays intact.
    general.add_bool_option(automatic_option, "Automatic update checks", "Check for newer stable Android releases when the launcher opens.", true, true);
    recompui::config::create_tab("Updates", "updates", [](recompui::ContextId context, recompui::Element* parent) {
        context.create_element<UpdatePage>(parent);
    });
}
void launcher_created() { if (automatic_enabled()) android::check(true); }
void launcher_updated(recompui::LauncherMenu*) {
    if (SDL_GetTicks64() < next_launcher_poll) return;
    next_launcher_poll = SDL_GetTicks64() + 500;
    State state = read_state();
    const bool other_modal = recompui::is_prompt_open() || recompui::config::get_config_modal()->is_open_now();
    if (notice_policy.take(state.available, state.status == "available", state.automatic, automatic_enabled(),
            ultramodern::is_game_started(), other_modal, android::can_show_notice())) {
        recompui::open_choice_prompt("Update available", "Tooie Recompiled " + state.available + " is available. View the release to see what's new and download the update.",
            "View Release", "Later", [] { android::open_release(); }, [] {},
            recompui::ButtonStyle::Primary, recompui::ButtonStyle::Secondary, true);
    }
}
}
#else
void tooie::updates::register_options(recomp::config::Config&) {}
void tooie::updates::launcher_created() {}
void tooie::updates::launcher_updated(recompui::LauncherMenu*) {}
#endif
