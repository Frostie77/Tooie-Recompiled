#include "android_touch_controls.h"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "elements/ui_label.h"
namespace {
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
        button->add_pressed_callback(tooie::touch::show_settings);
    }
};
}
void tooie::touch::register_options() {
    recompui::config::create_tab("Touch","touch",[](recompui::ContextId context,recompui::Element* parent) {
        context.create_element<TouchPage>(parent);
    });
}
