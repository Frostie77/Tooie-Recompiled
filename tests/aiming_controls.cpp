// Actual generated aiming consumers and Config/file persistence. Peripheral
// camera/timer/math services use controlled values; this does not run a ROM.
#include "aiming_controls.h"
#include "tooie_recomp.h"
#include "librecomp/config.hpp"
#include "librecomp/game.hpp"
#include <algorithm>
#include <array>
#include <bit>
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
void parent_func_808000B4_baeggaim(uint8_t*, recomp_context*);
void parent_func_808002A4_bafpctrl(uint8_t*, recomp_context*);
void parent_func_80804848_bsfirstp(uint8_t*, recomp_context*);
void func_8009EF28(uint8_t*, recomp_context*);
void func_8009EF34(uint8_t*, recomp_context*);
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
static uint32_t camera_mode = 2, input_disabled = 0;
static unsigned scenarios;
static std::array<uint32_t, 3> submitted_angles{};
static std::array<uint32_t, 2> submitted_cursor{};
static uint32_t submitted_yaw;
static float dt = 1.0f / 60.0f;
static uint32_t bits(float v) { return std::bit_cast<uint32_t>(v); }
static float value(uint32_t v) { return std::bit_cast<float>(v); }
static void put(uint8_t* r, uint32_t p, uint32_t v) {
    CHECK(p >= 0x80000000 && p <= 0x807FFFFC && !(p & 3));
    std::memcpy(r + p - 0x80000000, &v, 4);
}
static uint32_t get(uint8_t* r, uint32_t p) {
    uint32_t v; std::memcpy(&v, r + p - 0x80000000, 4); return v;
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
extern "C" void _baeggcursor_entrypoint_3(uint8_t* r, recomp_context* c) {
    put(r, uint32_t(c->r5), bits(152.0f)); put(r, uint32_t(c->r6), bits(114.0f));
}
extern "C" void _baeggcursor_entrypoint_10(uint8_t*, recomp_context* c) { submitted_cursor = {uint32_t(c->r5), uint32_t(c->r6)}; }
extern "C" void func_800F0DC0(uint8_t*, recomp_context* c) { c->f0.fl = std::clamp(c->f12.fl, -c->f14.fl, c->f14.fl); }
extern "C" void func_800F2108(uint8_t*, recomp_context* c) { c->f0.fl = std::abs(c->f12.fl); }
// Button-pair reader: its output is independent of the joystick preference.
extern "C" void func_80804488_bsfirstp(uint8_t* r, recomp_context* c) { put(r, uint32_t(c->r7), bits(0.25f)); }

static recomp::config::Config make_config(bool confirmation = false) {
    recomp::config::Config c("General", "general", confirmation);
    c.add_number_option("joystick_deadzone", "Deadzone", "", 0, 1, 0.01, 2, false, 0.15);
    c.add_percent_number_option("rumble_strength", "Rumble", "", 25.0);
    tooie::input::add_aiming_option(c);
    return c;
}
static void set_invert(bool enabled) {
    auto c = make_config(); c.update_option_value("aiming_invert", enabled);
}
static void bridge_abi() {
    for (bool enabled : {false, true}) {
        set_invert(enabled);
        for (uint32_t sample : {0U, 0x80000000U, 0x3F800000U, 0xBF400000U, 1U, 0x7F7FFFFFU, 0x7F800000U, 0xFF800000U, 0x7FC12345U}) {
            recomp_context actual{}, expected{};
            std::memset(&actual, 0xA5, sizeof(actual)); actual.f0.u32l = sample; expected = actual;
            auto magnitude = sample & 0x7FFFFFFFU;
            if (enabled && magnitude && magnitude < 0x7F800000U) expected.f0.u32l ^= 0x80000000U;
            std::array<uint8_t, 64> ram{}; auto prior = ram;
            tooie_aiming_invert(ram.data(), &actual);
            CHECK(!std::memcmp(&actual, &expected, sizeof(actual)) && ram == prior);
        }
    }
}

static void consumers() {
    std::vector<uint8_t> ram(0x800000), reference(0x800000);
    addresses[22] = int32_t(0x80200000); // Controlled relocation/constant for the real smoothing helper.
    const std::array<recomp_func_t*, 3> funcs = {func_808000B4_baeggaim, func_808002A4_bafpctrl, func_80804848_bsfirstp};
    const std::array<recomp_func_t*, 3> parents = {parent_func_808000B4_baeggaim, parent_func_808002A4_bafpctrl, parent_func_80804848_bsfirstp};
    for (bool enabled : {false, true}) for (float y : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) for (float x : {-0.75f, 0.0f, 0.75f}) {
        set_invert(enabled);
        for (unsigned consumer = 0; consumer < 3; ++consumer) for (int mode : {-1, 0, 1, 2}) for (unsigned flag : {0U, 1U}) {
            std::fill(ram.begin(), ram.end(), 0);
            put(ram.data(), player + 0x128, stick); put(ram.data(), player + 0x188, firstp);
            put(ram.data(), stick + 0x54, bits(x)); put(ram.data(), stick + 0x58, bits(y));
            put(ram.data(), firstp + 0x118, flag); put(ram.data(), 0x80200470, bits(0.1f));
            recomp_context a{}, b{}; a.r4 = int32_t(player); a.r5 = consumer == 0 ? int32_t(0x80504000) : mode;
            a.r29 = int32_t(0x80044000); a.r16 = int32_t(0x80333330); a.r31 = int32_t(0x80001234); b = a;
            camera_mode = uint32_t(mode); input_disabled = flag;
            reference = ram;
            const bool reads_pitch = consumer != 2 || mode == 1 || (mode == 0 && flag != 0);
            if (enabled && reads_pitch && y != 0) put(reference.data(), stick + 0x58, bits(-y));
            submitted_angles = {}; submitted_cursor = {}; submitted_yaw = 0;
            funcs[consumer](ram.data(), &a);
            const auto angles = submitted_angles; const auto cursor = submitted_cursor; const auto yaw = submitted_yaw;
            submitted_angles = {}; submitted_cursor = {}; submitted_yaw = 0;
            parents[consumer](reference.data(), &b);
            // Only the reference fixture's input differs; the production hook must never write it.
            put(reference.data(), stick + 0x58, bits(y));
            CHECK(ram == reference);
            CHECK(!std::memcmp(&a, &b, sizeof(a)));
            CHECK(angles == submitted_angles && cursor == submitted_cursor && yaw == submitted_yaw);
            CHECK(uint32_t(a.r29) == 0x80044000 && uint32_t(a.r16) == 0x80333330 && uint32_t(a.r31) == 0x80001234);
            CHECK(get(ram.data(), stick + 0x54) == bits(x) && get(ram.data(), stick + 0x58) == bits(y));
            // Outside aiming, the original shared getters retain both signs.
            a.r4 = int32_t(player); func_8009EF34(ram.data(), &a); CHECK(a.f0.u32l == bits(y));
            a.r4 = int32_t(player); func_8009EF28(ram.data(), &a); CHECK(a.f0.u32l == bits(x));
            ++scenarios;
        }
    }
}

static void persistence() {
    using namespace recomp::config;
    auto c = make_config(); CHECK(c.load_config());
    const auto& option = c.get_option("aiming_invert");
    CHECK(option.name == "Invert Aiming y-axis" && !option.hidden && option.type == ConfigOptionType::Bool);
    CHECK(!std::get<bool>(c.get_option_value("aiming_invert")));
    c.update_option_value("joystick_deadzone", 0.27); c.update_option_value("rumble_strength", 60.0);
    c.update_option_value("aiming_invert", true); CHECK(c.save_config());
    auto reloaded = make_config(); CHECK(reloaded.load_config());
    CHECK(std::get<bool>(reloaded.get_option_value("aiming_invert")));
    CHECK(std::get<double>(reloaded.get_option_value("joystick_deadzone")) == 0.27);
    CHECK(std::get<double>(reloaded.get_option_value("rumble_strength")) == 60.0);
    recomp_context input{}; input.f0.fl = 0.5f; tooie_aiming_invert(nullptr, &input); CHECK(input.f0.fl == -0.5f);
    reloaded.update_option_value("aiming_invert", false); CHECK(reloaded.save_config());
    auto off = make_config(); CHECK(off.load_config()); input.f0.fl = 0.5f; tooie_aiming_invert(nullptr, &input); CHECK(input.f0.fl == 0.5f);
    // Previous configs without this field and invalid types use the default.
    for (auto invalid : {nlohmann::json(), nlohmann::json("true"), nlohmann::json(1), nlohmann::json::array()}) {
        auto data = off.get_json_config(); data["aiming_invert"] = invalid;
        std::ofstream(config_path / "general.json") << data;
        auto legacy = make_config(); CHECK(legacy.load_config());
        CHECK(!std::get<bool>(legacy.get_option_value("aiming_invert")));
        CHECK(std::get<double>(legacy.get_option_value("joystick_deadzone")) == 0.27);
    }
    auto data = off.get_json_config(); data.erase("aiming_invert"); std::ofstream(config_path / "general.json") << data;
    auto missing = make_config(); CHECK(missing.load_config()); CHECK(!std::get<bool>(missing.get_option_value("aiming_invert")));
    // Temporary previews must not leak into the running game's setting.
    auto temporary = make_config(true); temporary.update_option_value("aiming_invert", true);
    input.f0.fl = 0.5f; tooie_aiming_invert(nullptr, &input); CHECK(input.f0.fl == 0.5f);
    temporary.revert_temp_config(); CHECK(!std::get<bool>(temporary.get_temp_option_value("aiming_invert")));
}

int main(int argc, char** argv) {
    CHECK(argc == 2); config_path = argv[1]; std::filesystem::create_directories(config_path);
    bridge_abi(); consumers(); persistence();
    std::printf("PASS: %u actual generated aiming/first-person cases: signed/neutral Y, independent X, walking and mode gates, parent-equivalent OFF, RAM/ABI restoration\n", scenarios);
    std::puts("PASS: production General config defaults, visible Invert Aiming y-axis, immediate changes, ON/OFF disk reload, existing values, invalid/missing keys and temporary cancellation");
}
