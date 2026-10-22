#include "modding.h"
#include "recompconfig.h"
#include "recompui.h"

// No ROM addresses, game-function hooks, native libraries or save writes.
RECOMP_CALLBACK("*", recomp_on_init)
void tooie_mod_check_start(void) {
    unsigned long palette = recomp_get_config_u32("badge_colour");
    RecompuiContext context = recompui_create_context();
    recompui_open_context(context);
    RecompuiResource root = recompui_context_root(context);
    RecompuiResource label = recompui_create_label(
        context, root,
        palette == 1 ? "Tooie mod check: BLUE" : "Tooie mod check: GOLD",
        LABELSTYLE_NORMAL);
    const RecompuiColor background = {16, 18, 28, 224};
    const RecompuiColor gold = {255, 210, 64, 255};
    const RecompuiColor blue = {80, 180, 255, 255};
    recompui_set_position(label, POSITION_ABSOLUTE);
    recompui_set_left(label, 24.0f, UNIT_DP);
    recompui_set_top(label, 24.0f, UNIT_DP);
    recompui_set_padding(label, 12.0f, UNIT_DP);
    recompui_set_font_size(label, 28.0f, UNIT_DP);
    recompui_set_background_color(label, &background);
    recompui_set_color(label, palette == 1 ? &blue : &gold);
    recompui_set_context_captures_input(context, 0);
    recompui_set_context_captures_mouse(context, 0);
    recompui_close_context(context);
    recompui_show_context(context);
}
