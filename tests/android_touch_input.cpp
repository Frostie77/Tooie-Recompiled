#include "../src/touch_input.h"
#include <cassert>
#include <limits>
#include <thread>
static bool started=true, disabled=false, binding=false;
namespace ultramodern { bool is_game_started() { return started; } }
namespace recompinput {
bool game_input_disabled() { return disabled; }
bool all_input_disabled() { return binding; }
namespace profiles {
bool get_n64_input(int player, uint16_t* buttons, float* x, float* y) {
    *buttons=disabled?0:0x20; *x=disabled?0:.25f; *y=disabled?0:-.4f; return player==0;
}
}
}
int main() {
    using namespace tooie::touch;
    auto& state=input_state();
    uint16_t buttons; float x,y;
    assert(read_n64_input(0,&buttons,&x,&y) && buttons==0x20 && x==.25f && y==-.4f);
    state.set_enabled(true); state.set_gameplay(true);
    auto epoch=state.snapshot().epoch;
    assert(state.publish(epoch,0x8000,.7f,.2f));
    assert(read_n64_input(0,&buttons,&x,&y) && buttons==0x8020 && x==.7f && y==-.4f);
    assert(!read_n64_input(1,&buttons,&x,&y) && buttons==0x20 && x==.25f);
    disabled=true; assert(read_n64_input(0,&buttons,&x,&y) && buttons==0 && x==0 && y==0);
    assert(!state.publish(epoch,0x8000,1,1));
    disabled=false; read_n64_input(0,&buttons,&x,&y); assert(buttons==0x20 && x==.25f);
    epoch=state.snapshot().epoch;
    assert(state.publish(epoch,-1,2,std::numeric_limits<float>::quiet_NaN()));
    auto value=state.snapshot(); assert(value.buttons==button_mask && value.x==1 && value.y==0);
    auto status=state.cancel(); assert(status&allowed_bit); assert(!state.publish(epoch,0x8000,1,1));
    value=state.snapshot(); assert(value.buttons==0 && value.x==0 && value.y==0);
    binding=true; read_n64_input(0,&buttons,&x,&y); assert(!state.snapshot().allowed);
    binding=false; started=false; read_n64_input(0,&buttons,&x,&y); assert(!state.snapshot().allowed);
    started=true; state.set_gameplay(true); state.set_enabled(false);
    assert(!state.publish(state.snapshot().epoch,0x8000,1,1));
    state.set_enabled(true);
    InputState concurrent; concurrent.set_enabled(true); concurrent.set_gameplay(true);
    std::thread producer([&] { for(int i=0;i<20000;i++) { auto e=concurrent.snapshot().epoch; concurrent.publish(e,0x8000,1,.5f); } });
    for(int i=0;i<20000;i++) {
        if(i%7==0)concurrent.cancel();
        auto s=concurrent.snapshot(); assert((s.buttons==0 && s.x==0 && s.y==0)||(s.buttons==0x8000 && s.x==1 && s.y==.5f));
    }
    producer.join();
}
