#include "android_driver_settings.h"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "elements/ui_label.h"
#include <SDL.h>
#include <utility>

namespace {
class DriverSection : public recompui::Element {
    std::array<recompui::Label*,3> labels{};
    std::array<std::string,4> previous{};
    uint64_t nextPoll=0;
    void refresh() {
        auto state=tooie::android::gpu::settings_state();
        for(size_t i=0;i<labels.size();i++)if(state[i]!=previous[i])labels[i]->set_text(state[i]);
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
        for(auto& label:labels)label=context.create_element<recompui::Label>(this,"Reading driver…",recompui::LabelStyle::Normal);
        auto* row=context.create_element<recompui::Element>(this,0,"div");
        row->set_display(recompui::Display::Flex);row->set_gap(12.0f);row->set_flex_wrap(recompui::FlexWrap::Wrap);
        for(auto [text,action]:{std::pair{"Select custom driver…",1},std::pair{"Reset to System Default",2},std::pair{"Driver details / restart",0}}) {
            auto* button=context.create_element<recompui::Button>(row,text,recompui::ButtonStyle::Secondary,recompui::ButtonSize::Medium);
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
