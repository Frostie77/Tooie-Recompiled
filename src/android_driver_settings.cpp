#include "android_driver_settings.h"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "elements/ui_label.h"
#include <SDL.h>
#include <utility>

namespace {
constexpr std::array<std::string_view,3> fieldPrefixes{"Selected: ","Loaded: ","Status: "};
constexpr recompui::Color driverValueBlue{0x78,0xBE,0xFF,0xFF};

class DriverButton : public recompui::Button {
protected:
    void process_event(const recompui::Event& event) override {
        recompui::Button::process_event(event);
        // Match the existing config options: controller focus must reveal the
        // selected control in the shared Graphics scroll container.
        if(event.type==recompui::EventType::Focus && std::get<recompui::EventFocus>(event.variant).active)
            scroll_into_view();
    }
public:
    using recompui::Button::Button;
};

class DriverSection : public recompui::Element {
    std::array<recompui::Label*,3> labels{};
    std::array<std::string,4> previous{};
    uint64_t nextPoll=0;
    void refresh() {
        auto state=tooie::android::gpu::settings_state();
        for(size_t i=0;i<labels.size();i++)if(state[i]!=previous[i]) {
            std::string_view value=state[i];
            if(value.starts_with(fieldPrefixes[i]))value.remove_prefix(fieldPrefixes[i].size());
            labels[i]->set_text(value);
        }
        previous=std::move(state);
    }
protected:
    std::string_view get_type_name() override {return "TooieGraphicsDriver";}
    void process_event(const recompui::Event& event) override {
        if(event.type!=recompui::EventType::Update)return;
        if(SDL_GetTicks64()>=nextPoll) {nextPoll=SDL_GetTicks64()+500;refresh();}
        queue_update();
    }
public:
    DriverSection(recompui::ResourceId rid,recompui::Element* parent):Element(rid,parent,recompui::Events(recompui::EventType::Update)) {
        set_display(recompui::Display::Flex);set_flex_direction(recompui::FlexDirection::Column);
        set_gap(12.0f);set_padding_top(24.0f);set_width(100.0f,recompui::Unit::Percent);
        auto context=recompui::get_current_context();
        context.create_element<recompui::Label>(this,"Graphics driver",recompui::LabelStyle::Large);
        auto* fields=context.create_element<recompui::Element>(this,0,"div");
        fields->set_display(recompui::Display::Flex);fields->set_flex_direction(recompui::FlexDirection::Column);
        fields->set_gap(24.0f);fields->set_width(100.0f,recompui::Unit::Percent);
        for(size_t i=0;i<labels.size();i++) {
            auto* line=context.create_element<recompui::Element>(fields,0,"div");
            line->set_display(recompui::Display::Flex);line->set_gap(8.0f);line->set_width(100.0f,recompui::Unit::Percent);
            auto* prefix=context.create_element<recompui::Label>(line,std::string(fieldPrefixes[i]),recompui::LabelStyle::Normal);
            prefix->set_flex_shrink(0.0f);
            labels[i]=context.create_element<recompui::Label>(line,"Reading…",recompui::LabelStyle::Normal);
            labels[i]->set_color(driverValueBlue);labels[i]->set_flex_grow(1.0f);labels[i]->set_flex_shrink(1.0f);
            labels[i]->set_flex_basis(0.0f);labels[i]->set_min_width(0.0f);
        }
        auto* row=context.create_element<recompui::Element>(this,0,"div");
        row->set_display(recompui::Display::Flex);row->set_gap(12.0f);row->set_flex_wrap(recompui::FlexWrap::Wrap);
        for(auto [text,action]:{std::pair{"Select custom driver…",1},std::pair{"Reset to System Default",2},std::pair{"Driver details / restart",0}}) {
            auto* button=context.create_element<DriverButton>(row,text,recompui::ButtonStyle::Secondary,recompui::ButtonSize::Medium);
            button->add_pressed_callback([action]{tooie::android::gpu::show_settings(action);});
        }
        context.create_element<recompui::Label>(this,"Driver changes take effect after restarting the game.",recompui::LabelStyle::Small);
        refresh();queue_update();
    }
};
}
void tooie::android::gpu::register_settings() {
    recompui::config::append_tab_contents(recompui::config::graphics::id,[](recompui::ContextId context,recompui::Element* parent){context.create_element<DriverSection>(parent);});
}
