// Complete retained guest consumers, real Config persistence and production
// controller profiles. Only devices, camera services and timers are fixtures.
#include "aiming_controls.h"
#include "controller_input.h"
#include "tooie_recomp.h"
#include "librecomp/config.hpp"
#include "librecomp/game.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "aiming failure line %d: %s\n", __LINE__, #x); std::exit(23); } } while (0)
extern "C" {
void func_808000B4_baeggaim(uint8_t*, recomp_context*);
void func_808002A4_bafpctrl(uint8_t*, recomp_context*);
void func_80804848_bsfirstp(uint8_t*, recomp_context*);
void bafly_entrypoint_10(uint8_t*, recomp_context*);
void parent_func_808000B4_baeggaim(uint8_t*, recomp_context*);
void parent_func_808002A4_bafpctrl(uint8_t*, recomp_context*);
void parent_func_80804848_bsfirstp(uint8_t*, recomp_context*);
void parent_bafly_entrypoint_10(uint8_t*, recomp_context*);
void func_8009EF28(uint8_t*, recomp_context*);
void func_8009EF34(uint8_t*, recomp_context*);
void func_8009E6F8(uint8_t*, recomp_context*);
void func_800919D0(uint8_t*, recomp_context*);
void func_800BCCF4(uint8_t*, recomp_context*);
void bafpctrl_entrypoint_1(uint8_t*, recomp_context*);
void bafpctrl_entrypoint_4(uint8_t*, recomp_context*);
void bafpctrl_entrypoint_6(uint8_t*, recomp_context*);
}
static std::filesystem::path config_path;
namespace recomp {
std::filesystem::path get_config_path() { return config_path; }
const Version& get_project_version() { static const Version v{1, 0, 0, ""}; return v; }
}
static int32_t addresses[855]{};
int32_t* section_addresses = addresses;
static constexpr uint32_t player = 0x80500000, stick = 0x80501000;
static constexpr uint32_t firstp = 0x80502000, camera = 0x80503000;
static constexpr uint32_t aim = 0x80505000, state = 0x80506000, controller = 0x80507000, device = 0x80508000;
static constexpr std::array keys = {"aiming_invert", "flight_invert", "aiming_swap_sticks"};
enum Scope { Stationary, Flight, Rifle, OtherMoving, Inactive };
static uint32_t camera_mode = 2, input_disabled = 0;
static unsigned scenarios;
static std::array<uint32_t, 3> submitted_angles{};
static std::array<uint32_t, 2> submitted_cursor{};
static uint32_t submitted_yaw, submitted_pitch;
static float dt = 1.0f / 60.0f;
static uint32_t bits(float v) { return std::bit_cast<uint32_t>(v); }
static void put(uint8_t* r, uint32_t p, uint32_t v) {
    CHECK(p >= 0x80000000 && p <= 0x807FFFFC && !(p & 3));
    std::memcpy(r + p - 0x80000000, &v, 4);
}
static uint32_t get(uint8_t* r, uint32_t p) {
    uint32_t v; std::memcpy(&v, r + p - 0x80000000, 4); return v;
}
static void byte(uint8_t* r, uint32_t p, uint8_t value) { r[(p - 0x80000000) ^ 3] = value; }
static void context(uint8_t* ram, Scope scope, uint32_t handler = 0) {
    put(ram, player + 0x90, aim); put(ram, player + 0x120, state);
    put(ram, player + 0x40, controller); put(ram, controller, device);
    byte(ram, device + 4, 0); byte(ram, device + 5, 2); byte(ram, device + 6, 0);
    put(ram, state + 4, 37); // Arbitrary fixture ID: dispatch identity determines scope.
    if (!handler) handler = scope == Flight ? 0x80082950 : scope == Rifle ? 0x80084288 : 0x80083118;
    put(ram, 0x80117F38 + 37 * 12, handler);
    byte(ram, aim + 0x10, scope == Inactive ? 2 : 3);
    byte(ram, aim + 0x11, scope == Flight || scope == OtherMoving);
    byte(ram, aim + 0x13, 1);
    put(ram, 0x80135490, player);
}
extern "C" void func_800A4CA8(uint8_t*, recomp_context* c) { c->r2 = int32_t(camera); }
extern "C" void _ncba1p_entrypoint_10(uint8_t*, recomp_context* c) { c->r2 = camera_mode; }
extern "C" void _ncba1p_entrypoint_8(uint8_t* r, recomp_context* c) {
    for (unsigned i = 0; i < 3; ++i) put(r, uint32_t(c->r5) + i * 4, bits(i == 0 ? 5.0f : i == 1 ? 30.0f : 0.0f));
}
extern "C" void _ncba1p_entrypoint_7(uint8_t* r, recomp_context* c) {
    for (unsigned i = 0; i < 3; ++i) submitted_angles[i] = get(r, uint32_t(c->r5) + i * 4);
}
extern "C" void func_80095738(uint8_t*, recomp_context* c) { c->r2 = input_disabled; }
extern "C" void func_800D8FF8(uint8_t*, recomp_context* c) { c->f0.fl = dt; }
extern "C" void func_800D8FEC(uint8_t*, recomp_context* c) { c->r2 = 1; }
extern "C" void bafpctrl_entrypoint_3(uint8_t*, recomp_context* c) { c->f0.fl = 0.5f; }
extern "C" void _bsdronelook_entrypoint_2(uint8_t*, recomp_context* c) { c->f0.fl = std::clamp(c->f12.fl, -70.0f, 70.0f); }
extern "C" void func_8009C914(uint8_t*, recomp_context* c) { submitted_yaw = uint32_t(c->r5); }
extern "C" void func_8009BF5C(uint8_t*, recomp_context* c) { submitted_pitch = uint32_t(c->r5); }
extern "C" void _baeggcursor_entrypoint_3(uint8_t* r, recomp_context* c) {
    put(r, uint32_t(c->r5), bits(152.0f)); put(r, uint32_t(c->r6), bits(114.0f));
}
extern "C" void _baeggcursor_entrypoint_10(uint8_t*, recomp_context* c) { submitted_cursor = {uint32_t(c->r5), uint32_t(c->r6)}; }
extern "C" void func_800F0DC0(uint8_t*, recomp_context* c) { c->f0.fl = std::clamp(c->f12.fl, -c->f14.fl, c->f14.fl); }
extern "C" void func_800F2108(uint8_t*, recomp_context* c) { c->f0.fl = std::abs(c->f12.fl); }
extern "C" void func_80804488_bsfirstp(uint8_t* r, recomp_context* c) { put(r, uint32_t(c->r7), bits(0.25f)); }

static recomp::config::Config make_config(bool confirmation = false) {
    recomp::config::Config c("General", "general", confirmation);
    c.add_number_option("joystick_deadzone", "Deadzone", "", 0, 1, 0.01, 2, false, 0.15);
    c.add_percent_number_option("rumble_strength", "Rumble", "", 25.0);
    tooie::input::add_aiming_option(c);
    return c;
}
static void settings(bool aiming, bool flight, bool swap = false) {
    auto c = make_config();
    c.update_option_value(keys[0], aiming); c.update_option_value(keys[1], flight); c.update_option_value(keys[2], swap);
}
static void bridge_abi() {
    std::vector<uint8_t> ram(0x800000);
    for (bool a : {false, true}) for (bool f : {false, true}) for (Scope scope : {Stationary, Flight, Rifle, OtherMoving, Inactive}) {
        settings(a, f); context(ram.data(), scope);
        for (uint32_t sample : {0U, 0x80000000U, 0x3F800000U, 0xBF400000U, 1U, 0x7F7FFFFFU, 0x7F800000U, 0xFF800000U, 0x7FC12345U}) {
            recomp_context actual{}, expected{};
            std::memset(&actual, 0xA5, sizeof(actual)); actual.f0.u32l = sample; expected = actual;
            const bool enabled = scope == Stationary ? a : scope == Flight && f;
            auto magnitude = sample & 0x7FFFFFFFU;
            if (enabled && magnitude && magnitude < 0x7F800000U) expected.f0.u32l ^= 0x80000000U;
            auto prior = ram;
            tooie_aiming_invert(ram.data(), &actual, player);
            CHECK(!std::memcmp(&actual, &expected, sizeof(actual)) && ram == prior);
        }
    }
}
static void consumers() {
    std::vector<uint8_t> ram(0x800000), reference(0x800000);
    addresses[22] = int32_t(0x80200000);
    const std::array<recomp_func_t*, 4> funcs = {func_808000B4_baeggaim, func_808002A4_bafpctrl, func_80804848_bsfirstp, bafly_entrypoint_10};
    const std::array<recomp_func_t*, 4> parents = {parent_func_808000B4_baeggaim, parent_func_808002A4_bafpctrl, parent_func_80804848_bsfirstp, parent_bafly_entrypoint_10};
    for (bool aiming : {false, true}) for (bool flight : {false, true}) for (Scope scope : {Stationary, Flight, Rifle, OtherMoving, Inactive}) {
        settings(aiming, flight);
        for (float y : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) for (float x : {-0.75f, 0.0f, 0.75f}) {
            for (unsigned consumer = 0; consumer < 4; ++consumer) for (unsigned blocked : {0U, 1U}) {
                std::fill(ram.begin(), ram.end(), 0); context(ram.data(), scope);
                put(ram.data(), player + 0x128, stick); put(ram.data(), player + 0x188, firstp);
                put(ram.data(), stick + 0x54, bits(x)); put(ram.data(), stick + 0x58, bits(y));
                put(ram.data(), firstp + 0x118, 1); put(ram.data(), 0x80200470, bits(0.1f));
                recomp_context a{}, b{}; a.r4 = int32_t(player);
                a.r5 = int32_t(consumer == 0 ? 0x80504000U : consumer == 3 ? bits(300.0f) : consumer == 2 ? (blocked ? 0U : 1U) : 1U);
                a.r6 = bits(80.0f); a.r29 = int32_t(0x80044000); a.r16 = int32_t(0x80333330); a.r31 = int32_t(0x80001234); b = a;
                camera_mode = blocked ? 0 : 2; input_disabled = blocked;
                reference = ram;
                const bool enabled = consumer == 2 ? false : consumer == 3 ? scope == Flight && flight : scope == Flight ? flight : scope == Stationary && aiming;
                if (enabled && y != 0) put(reference.data(), stick + 0x58, bits(-y));
                submitted_angles = {}; submitted_cursor = {}; submitted_yaw = submitted_pitch = 0;
                funcs[consumer](ram.data(), &a);
                const auto angles = submitted_angles; const auto cursor = submitted_cursor; const auto yaw = submitted_yaw; const auto pitch = submitted_pitch;
                submitted_angles = {}; submitted_cursor = {}; submitted_yaw = submitted_pitch = 0;
                parents[consumer](reference.data(), &b);
                put(reference.data(), stick + 0x58, bits(y));
                CHECK(ram == reference);
                CHECK(!std::memcmp(&a, &b, sizeof(a)));
                CHECK(angles == submitted_angles && cursor == submitted_cursor && yaw == submitted_yaw && pitch == submitted_pitch);
                CHECK(uint32_t(a.r29) == 0x80044000 && uint32_t(a.r16) == 0x80333330 && uint32_t(a.r31) == 0x80001234);
                a.r4 = int32_t(player); func_8009EF34(ram.data(), &a); CHECK(a.f0.u32l == bits(y));
                a.r4 = int32_t(player); func_8009EF28(ram.data(), &a); CHECK(a.f0.u32l == bits(x));
                ++scenarios;
            }
        }
    }
    // Include the unmodified rifle walking read and all FPS mode gates, as well
    // as the two aiming channels above. These must ignore every new preference.
    settings(true, true, true);
    for (int mode : {-1, 0, 1, 2}) for (unsigned modifier : {0U, 1U}) for (float y : {-.5f, .5f}) {
        std::fill(ram.begin(), ram.end(), 0); context(ram.data(), Rifle);
        put(ram.data(), player + 0x128, stick); put(ram.data(), player + 0x188, firstp);
        put(ram.data(), stick + 0x54, bits(.75f)); put(ram.data(), stick + 0x58, bits(y));
        put(ram.data(), firstp + 0x118, modifier);
        recomp_context a{}, b{}; a.r4 = int32_t(player); a.r5 = mode;
        a.r29 = int32_t(0x80044000); a.r16 = int32_t(0x80333330); a.r31 = int32_t(0x80001234); b = a;
        reference = ram;
        func_80804848_bsfirstp(ram.data(), &a); parent_func_80804848_bsfirstp(reference.data(), &b);
        CHECK(ram == reference && !std::memcmp(&a, &b, sizeof(a)));
        ++scenarios;
    }
}

// Fixtures for physical devices. Profile lookup, both binding slots, analog /
// digital aggregation, deadzone and the game callback use production code.
static bool started = true, disabled = false, binding = false;
static std::array<std::array<float, 6>, 4> physical_axes{};
static std::array<bool, SDL_CONTROLLER_BUTTON_MAX> physical_buttons{};
static std::array<bool, SDL_NUM_SCANCODES> keyboard{};
namespace ultramodern { bool is_game_started() { return started; } }
namespace recompui::config::general { double get_joystick_deadzone() { return 0.0; } }
namespace recompinput {
bool game_input_disabled() { return disabled; }
bool all_input_disabled() { return binding; }
namespace players { bool is_single_player_mode() { return false; } }
float get_input_analog(int index, const InputField& field) {
    if (field.input_type == InputType::ControllerAnalog) {
        int axis = std::abs(field.input_id) - 1;
        CHECK(axis >= 0 && axis < 6);
        return std::max(0.0f, physical_axes.at(index).at(axis) * (field.input_id > 0 ? 1 : -1));
    }
    if (field.input_type == InputType::Keyboard) return keyboard.at(field.input_id) ? 1.0f : 0.0f;
    return 0.0f;
}
bool get_input_digital(int index, const InputField& field) {
    return field.input_type == InputType::ControllerDigital ? physical_buttons.at(field.input_id) : get_input_analog(index, field) >= 0.5f;
}
}
static void profile_swapping() {
    using namespace recompinput;
    std::vector<uint8_t> ram(0x800000);
    tooie::input::initialize_controls(ram.data());
    auto profile = profiles::add_input_profile("test-pad", "Test pad", InputDevice::Controller, true);
    auto kb = profiles::add_input_profile("test-kb", "Test keyboard", InputDevice::Keyboard, true);
    profiles::set_input_profile_for_player(0, profile, InputDevice::Controller);
    profiles::set_input_profile_for_player(0, kb, InputDevice::Keyboard);
    auto axis = [&](GameInput input, SDL_GameControllerAxis source, bool positive) { profiles::set_input_binding(profile, input, 0, InputField::controller_analog(source, positive)); };
    axis(GameInput::X_AXIS_POS, SDL_CONTROLLER_AXIS_LEFTX, true); axis(GameInput::X_AXIS_NEG, SDL_CONTROLLER_AXIS_LEFTX, false);
    axis(GameInput::Y_AXIS_POS, SDL_CONTROLLER_AXIS_LEFTY, false); axis(GameInput::Y_AXIS_NEG, SDL_CONTROLLER_AXIS_LEFTY, true);
    axis(GameInput::C_RIGHT, SDL_CONTROLLER_AXIS_RIGHTX, true); axis(GameInput::C_LEFT, SDL_CONTROLLER_AXIS_RIGHTX, false);
    axis(GameInput::C_DOWN, SDL_CONTROLLER_AXIS_RIGHTY, true); axis(GameInput::C_UP, SDL_CONTROLLER_AXIS_RIGHTY, false);
    axis(GameInput::Z, SDL_CONTROLLER_AXIS_TRIGGERLEFT, true);
    profiles::set_input_binding(profile, GameInput::A, 1, InputField::controller_digital(SDL_CONTROLLER_BUTTON_A));
    profiles::set_input_binding(profile, GameInput::R, 0, InputField::controller_digital(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
    profiles::set_input_binding(kb, GameInput::Y_AXIS_POS, 0, InputField::keyboard(SDL_SCANCODE_W));
    physical_buttons[SDL_CONTROLLER_BUTTON_A] = physical_buttons[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = true;
    physical_axes[0] = {.2f, -.8f, -.9f, .6f, .75f, 0};
    std::vector<InputField> saved;
    for (size_t i = 0; i < num_game_inputs; ++i) for (size_t j = 0; j < 2; ++j) saved.push_back(profiles::get_input_binding(profile, GameInput(i), j));
    for (bool swap : {false, true}) for (bool invert : {false, true}) for (Scope scope : {Stationary, Flight, Rifle, OtherMoving, Inactive}) {
        context(ram.data(), scope); settings(invert, !invert, swap);
        uint16_t buttons; float x, y;
        CHECK(tooie::input::read_controller_input(0, &buttons, &x, &y));
        const bool exchanged = swap && scope == Stationary;
        CHECK(x == (exchanged ? -.9f : .2f) && y == (exchanged ? -.6f : .8f));
        CHECK(buttons == (uint16_t(0xA010) | (exchanged ? 0x8 : 0x6)));
        put(ram.data(), player + 0x128, stick); put(ram.data(), stick + 0x58, bits(y));
        recomp_context c{}; c.f0.fl = y;
        tooie_aiming_invert(ram.data(), &c, player);
        const bool flip = scope == Stationary ? invert : scope == Flight && !invert;
        CHECK(c.f0.fl == (flip ? -y : y)); // Inversion follows the exchanged aiming stick.
    }
    settings(true, true, true); context(ram.data(), Stationary);
    // Every axis sign/quadrant and neutral, in both directions, through the
    // actual profile consumer. Physical buttons and triggers stay fixed.
    for (float lx : {-1.f, 0.f, 1.f}) for (float ly : {-1.f, 0.f, 1.f})
    for (float rx : {-1.f, 0.f, 1.f}) for (float ry : {-1.f, 0.f, 1.f}) {
        physical_axes[0] = {lx, ly, rx, ry, .75f, 0};
        uint16_t buttons; float x, y;
        tooie::input::read_controller_input(0, &buttons, &x, &y);
        uint16_t expected = 0xA010 | (lx < 0 ? 2 : lx > 0 ? 1 : 0) | (ly < 0 ? 8 : ly > 0 ? 4 : 0);
        CHECK(x == rx && y == -ry && buttons == expected);
    }
    physical_axes[0] = {.2f, -.8f, -.9f, .6f, .75f, 0};
    uint16_t buttons; float x, y;
    keyboard[SDL_SCANCODE_W] = true;
    tooie::input::read_controller_input(0, &buttons, &x, &y); CHECK(std::abs(y - .4f) < .00001f);
    keyboard[SDL_SCANCODE_W] = false;
    // Exit/re-enter and disable in-place; no profile edits or restoration pass.
    byte(ram.data(), aim + 0x10, 2); tooie::input::read_controller_input(0, &buttons, &x, &y); CHECK(x == .2f && y == .8f);
    byte(ram.data(), aim + 0x10, 3); tooie::input::read_controller_input(0, &buttons, &x, &y); CHECK(x == -.9f && y == -.6f);
    settings(true, true, false); tooie::input::read_controller_input(0, &buttons, &x, &y); CHECK(x == .2f && y == .8f);
    size_t index = 0;
    for (size_t i = 0; i < num_game_inputs; ++i) for (size_t j = 0; j < 2; ++j) CHECK(saved[index++] == profiles::get_input_binding(profile, GameInput(i), j));
    settings(true, true, true);
    for (uint32_t handler = 0x80084288; handler <= 0x800843B0; handler += 8) { context(ram.data(), Stationary, handler); CHECK(!tooie::input::swap_aiming_sticks(0)); }
    for (uint32_t handler = 0x80082950; handler <= 0x80082998; handler += 8) { context(ram.data(), Stationary, handler); CHECK(!tooie::input::swap_aiming_sticks(0)); }
    context(ram.data(), Stationary);
    CHECK(!tooie::input::swap_aiming_sticks(-1) && !tooie::input::swap_aiming_sticks(4) && !tooie::input::swap_aiming_sticks(1));
    byte(ram.data(), device + 6, 1); CHECK(tooie::input::swap_aiming_sticks(1) && !tooie::input::swap_aiming_sticks(0));
    recomp_context c{}; c.r4 = int32_t(player); c.r29 = int32_t(0x80044000);
    c.r5 = int32_t(0x80509000);
    put(ram.data(), device + 0x24, bits(.3f)); put(ram.data(), device + 0x28, bits(-.7f));
    func_800919D0(ram.data(), &c); CHECK(get(ram.data(), 0x80509000) == bits(.3f) && get(ram.data(), 0x80509004) == bits(-.7f));
    c.r4 = int32_t(device); func_800BCCF4(ram.data(), &c); CHECK(c.r2 == 1);
    c.r4 = int32_t(player); func_8009E6F8(ram.data(), &c); CHECK(c.r2 == 37);
    c.r4 = int32_t(player); bafpctrl_entrypoint_4(ram.data(), &c); CHECK(c.r2 == 3);
    c.r4 = int32_t(player); bafpctrl_entrypoint_1(ram.data(), &c); CHECK(c.r2 == 0);
    c.r4 = int32_t(player); bafpctrl_entrypoint_6(ram.data(), &c); CHECK(c.r2 == 1);
    put(ram.data(), state + 4, 41); put(ram.data(), 0x80117F38 + 41 * 12, 0x80083118);
    CHECK(tooie::input::swap_aiming_sticks(1));
    byte(ram.data(), device + 6, 0);
    byte(ram.data(), device + 5, 1); CHECK(!tooie::input::swap_aiming_sticks(0)); byte(ram.data(), device + 5, 2);
    byte(ram.data(), aim + 0x13, 0); CHECK(!tooie::input::swap_aiming_sticks(0)); byte(ram.data(), aim + 0x13, 1);
    started = false; tooie::input::read_controller_input(0, &buttons, &x, &y); CHECK(x == .2f && y == .8f); started = true;
    binding = true; tooie::input::read_controller_input(0, &buttons, &x, &y); CHECK(x == .2f && y == .8f); binding = false;
    disabled = true; tooie::input::read_controller_input(0, &buttons, &x, &y); CHECK(!buttons && !x && !y); disabled = false;
    for (auto invalid : {0U, 0x7FFFFFFCU, 0x807FFFFCU, 0x80500001U, 0xFFFFFFFFU}) {
        put(ram.data(), 0x80135490, invalid); CHECK(!tooie::input::swap_aiming_sticks(0));
    }
    context(ram.data(), Stationary); put(ram.data(), state + 4, 0xFFFFFFFF); CHECK(!tooie::input::swap_aiming_sticks(0));
    tooie::input::initialize_controls(nullptr); CHECK(!tooie::input::swap_aiming_sticks(0));
}
static void persistence() {
    using namespace recomp::config;
    std::vector<uint8_t> ram(0x800000); context(ram.data(), Stationary); tooie::input::initialize_controls(ram.data());
    auto c = make_config(); CHECK(c.load_config());
    const std::array names = {"Invert Aiming y-axis", "Invert Flight y-axis", "Swap Joystick Aiming Controls"};
    for (size_t i = 0; i < keys.size(); ++i) {
        const auto& option = c.get_option(keys[i]);
        CHECK(option.name == names[i] && !option.hidden && option.type == ConfigOptionType::Bool);
        CHECK(c.get_option(i + 2).id == keys[i]);
        CHECK(!std::get<bool>(c.get_option_value(keys[i])));
    }
    c.update_option_value("joystick_deadzone", 0.27); c.update_option_value("rumble_strength", 60.0);
    for (auto key : keys) c.update_option_value(key, true);
    CHECK(c.save_config());
    auto reloaded = make_config(); CHECK(reloaded.load_config());
    for (auto key : keys) CHECK(std::get<bool>(reloaded.get_option_value(key)));
    CHECK(std::get<double>(reloaded.get_option_value("joystick_deadzone")) == 0.27);
    CHECK(std::get<double>(reloaded.get_option_value("rumble_strength")) == 60.0);
    recomp_context input{}; input.f0.fl = 0.5f; tooie_aiming_invert(ram.data(), &input, player); CHECK(input.f0.fl == -0.5f);
    CHECK(tooie::input::swap_aiming_sticks(0));
    context(ram.data(), Flight); input.f0.fl = 0.5f; tooie_flight_invert(ram.data(), &input, player); CHECK(input.f0.fl == -0.5f);
    for (auto key : keys) reloaded.update_option_value(key, false);
    CHECK(reloaded.save_config());
    auto off = make_config(); CHECK(off.load_config()); input.f0.fl = 0.5f; tooie_flight_invert(ram.data(), &input, player); CHECK(input.f0.fl == 0.5f);
    for (auto key : keys) for (auto invalid : {nlohmann::json(), nlohmann::json("true"), nlohmann::json(1), nlohmann::json::array()}) {
        auto data = off.get_json_config(); data[key] = invalid; std::ofstream(config_path / "general.json") << data;
        auto legacy = make_config(); CHECK(legacy.load_config()); CHECK(!std::get<bool>(legacy.get_option_value(key)));
        CHECK(std::get<double>(legacy.get_option_value("joystick_deadzone")) == 0.27);
    }
    auto data = off.get_json_config(); data["aiming_invert"] = true; data.erase("flight_invert"); data.erase("aiming_swap_sticks");
    std::ofstream(config_path / "general.json") << data;
    auto legacy = make_config(); CHECK(legacy.load_config()); CHECK(std::get<bool>(legacy.get_option_value(keys[0])));
    CHECK(!std::get<bool>(legacy.get_option_value(keys[1])) && !std::get<bool>(legacy.get_option_value(keys[2])));
    auto temporary = make_config(true);
    for (auto key : keys) temporary.update_option_value(key, true);
    context(ram.data(), Stationary); input.f0.fl = 0.5f; tooie_aiming_invert(ram.data(), &input, player); CHECK(input.f0.fl == 0.5f);
    CHECK(!tooie::input::swap_aiming_sticks(0));
    context(ram.data(), Flight); input.f0.fl = 0.5f; tooie_flight_invert(ram.data(), &input, player); CHECK(input.f0.fl == 0.5f);
    temporary.revert_temp_config();
    for (auto key : keys) CHECK(!std::get<bool>(temporary.get_temp_option_value(key)));
    tooie::input::initialize_controls(nullptr);
}
int main(int argc, char** argv) {
    CHECK(argc == 2); config_path = argv[1]; std::filesystem::create_directories(config_path);
    bridge_abi(); consumers(); profile_swapping(); persistence();
    std::printf("PASS: %u complete guest-consumer cases: independent stationary/flight Y, coherent steering/reticle, rifle/walking exclusions, original OFF behavior and RAM/ABI preservation\n", scenarios);
    std::puts("PASS: actual controller profiles swap both axes and C-button roles only in stationary aiming; inversion follows aiming stick; exit/menu/player ownership and bindings preserved");
    std::puts("PASS: all three General settings default Off, persist independently, preserve old aiming preference and other values, reject invalid types and cancel previews");
}
