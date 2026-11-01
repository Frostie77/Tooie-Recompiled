// Retained generated dialogue controller and actual spline evaluation.
// Peripheral services are explicit spies; this does not run a ROM or GPU.
#include "tooie_recomp.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static const char* scenario = "setup";
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "dialogue camera probe failure: %s: line %d: %s\n", scenario, __LINE__, #x); std::exit(23); } } while (0)

extern "C" {
void func_80800000_gcdialogcamera(uint8_t*, recomp_context*);
void ncpoddialog_entrypoint_4(uint8_t*, recomp_context*);
void ncpoddialog_entrypoint_9(uint8_t*, recomp_context*);
void func_808015CC_ncpoddialog(uint8_t*, recomp_context*);
void mlspline_entrypoint_4(uint8_t*, recomp_context*);
}
static int32_t relocations[855]{};
int32_t* section_addresses = relocations;
static constexpr uint32_t camera = 0x80210000U;
static constexpr uint32_t owner = 0x80220000U;
static constexpr uint32_t pod = 0x80230000U;
static constexpr uint32_t parameters = 0x80240000U;
static constexpr uint32_t constants = 0x80300000U;
static constexpr uint32_t stack = 0x807F0000U;
static uint64_t guest(uint32_t p) { return static_cast<int64_t>(static_cast<int32_t>(p)); }
static uint32_t bits(float f) { return std::bit_cast<uint32_t>(f); }
static float number(uint32_t word) { return std::bit_cast<float>(word); }
static void put(uint8_t* r, uint32_t p, uint32_t v) {
    CHECK(p >= 0x80000000U && p <= 0x807FFFFCU && !(p & 3U));
    std::memcpy(r + p - 0x80000000U, &v, sizeof(v));
}
static uint32_t get(uint8_t* r, uint32_t p) {
    CHECK(p >= 0x80000000U && p <= 0x807FFFFCU && !(p & 3U));
    uint32_t v;
    std::memcpy(&v, r + p - 0x80000000U, sizeof(v));
    return v;
}
static uint8_t byte(uint8_t* rdram, uint32_t base, uint32_t offset) {
    return MEM_BU(offset, guest(base));
}
static void set_byte(uint8_t* rdram, uint32_t base, uint32_t offset, uint8_t value) {
    MEM_B(offset, guest(base)) = value;
}
static bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
struct Spies {
    unsigned advance = 0, evaluate = 0, submit = 0, fov = 0;
    unsigned request = 0, stage = 0, event = 0;
    uint32_t event_value = 0;
    uint32_t request_result = 4;
    float next_progress = 0.25f;
    std::array<float, 3> eye{}, rotation{};
};
static Spies spies;

extern "C" void _mlspline_entrypoint_13(uint8_t* r, recomp_context* c) {
    CHECK(c->r4 == 10 && uint32_t(c->r5) == camera + 0x58);
    CHECK(number(uint32_t(c->r6)) >= 0 && number(uint32_t(c->r6)) < 1);
    CHECK(std::isfinite(number(uint32_t(c->r7))) && number(uint32_t(c->r7)) > 0);
    CHECK(near(number(get(r, uint32_t(c->r29) + 0x10)), 1));
    CHECK(uint32_t(c->r7) != 0);
    ++spies.advance;
    c->f0.fl = spies.next_progress; // Controlled boundary, not a measured game rate.
}
extern "C" void _mlspline_entrypoint_4(uint8_t* r, recomp_context* c) {
    CHECK(c->r5 == 10);
    CHECK(uint32_t(c->r6) == camera + (spies.evaluate == 0 ? 0x58U : 0xD0U));
    ++spies.evaluate;
    mlspline_entrypoint_4(r, c);
}
extern "C" void func_80110024(uint8_t*, recomp_context* c) {
    CHECK(uint32_t(c->r4) == owner);
    c->r2 = guest(pod);
}
extern "C" void _ncpod_entrypoint_8(uint8_t* r, recomp_context* c) {
    CHECK(uint32_t(c->r4) == pod && uint32_t(c->r5) == camera + 0x170);
    CHECK(uint32_t(c->r6) == camera + 0x17C);
    for (unsigned i = 0; i < 3; ++i) {
        spies.eye[i] = number(get(r, uint32_t(c->r5) + i * 4));
        spies.rotation[i] = number(get(r, uint32_t(c->r6) + i * 4));
    }
    ++spies.submit;
}
extern "C" void _ncpod_entrypoint_9(uint8_t*, recomp_context* c) {
    CHECK(uint32_t(c->r4) == pod && near(number(uint32_t(c->r5)), 40));
    ++spies.fov;
}
extern "C" void _ncpod_entrypoint_5(uint8_t* r, recomp_context* c) {
    CHECK(uint32_t(c->r4) == owner && uint32_t(c->r5) == pod && c->r6 == 0x58);
    CHECK(get(r, uint32_t(c->r29) + 0x10) == 0);
    ++spies.request;
    c->r2 = spies.request_result;
}
extern "C" void func_80800338_ncpoddialog(uint8_t*, recomp_context* c) {
    CHECK(uint32_t(c->r4) == camera && uint32_t(c->r5) == owner && c->r6 == 1);
    ++spies.stage; // Transition handler is outside this probe's execution scope.
}
extern "C" void func_80801310_ncpoddialog(uint8_t*, recomp_context*) {
    CHECK(false); // Uninitialized camera setup is not a controlled case here.
}
extern "C" void func_8009055C(uint8_t*, recomp_context* c) {
    CHECK(c->r4 == 0x64);
    ++spies.event;
    spies.event_value = uint32_t(c->r5);
}

static recomp_context context() {
    recomp_context c{};
    c.r4 = guest(camera);
    c.r5 = guest(owner);
    c.r29 = guest(stack);
    c.r16 = guest(0x80550000U);
    return c;
}
static void setup(uint8_t* r) {
    spies = {};
    set_byte(r, camera, 0x211, 3);
    set_byte(r, camera, 0x212, 0);
    set_byte(r, camera, 0x213, 0);
    set_byte(r, camera, 0x216, 0);
    put(r, camera + 0x20C, bits(0));
    relocations[776] = static_cast<int32_t>(constants);
    // Synthetic rate thresholds/constants, not bytes obtained from a ROM.
    put(r, constants + 0x2A00, bits(50));
    put(r, constants + 0x2A04, bits(0.25f));
    put(r, constants + 0x2A08, bits(0.75f));
    put(r, 0x8012C760U, bits(1.0f / 30.0f)); // Actual dt getter's global.
    for (unsigned i = 0; i < 10; ++i) {
        for (unsigned component = 0; component < 3; ++component) {
            put(r, camera + 0x58 + i * 12 + component * 4, bits(i * 10.0f + component));
            put(r, camera + 0xD0 + i * 12 + component * 4, bits(i * 2.0f + component));
        }
    }
}
static void motion(uint8_t* r, uint8_t kind, float next, const char* name) {
    scenario = name;
    setup(r);
    set_byte(r, camera, 0x213, kind);
    spies.next_progress = next;
    auto c = context();
    func_808015CC_ncpoddialog(r, &c);
    const float weight = kind == 1 ? 1 : next;
    CHECK(near(number(get(r, camera + 0x20C)), weight));
    CHECK(spies.advance == (kind == 1 ? 0U : 1U));
    CHECK(spies.evaluate == 2 && spies.submit == 1 && spies.fov == 1);
    CHECK(spies.request == 0 && spies.stage == 0);
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(near(spies.eye[i], 90 * weight + i));
        CHECK(near(spies.rotation[i], 18 * weight + i));
    }
    CHECK(c.r29 == guest(stack) && c.r16 == guest(0x80550000U) && c.r2 == 0);
}
static void request(uint8_t* r, uint8_t pose, uint8_t kind) {
    auto c = context();
    c.r6 = pose;
    c.r7 = kind;
    ncpoddialog_entrypoint_9(r, &c);
    CHECK(c.r29 == guest(stack) && c.r16 == guest(0x80550000U));
}
static void request_cases(uint8_t* r) {
    scenario = "same-pose";
    setup(r);
    request(r, 3, 1);
    CHECK(byte(r, camera, 0x212) == 0 && byte(r, camera, 0x213) == 0);
    CHECK(spies.event == 0 && spies.request == 0);

    scenario = "changed-pose";
    setup(r);
    request(r, 4, 1);
    CHECK(byte(r, camera, 0x211) == 4 && byte(r, camera, 0x212) == 1);
    CHECK(byte(r, camera, 0x213) == 1 && spies.event == 1 && spies.event_value == 0x10004);

    scenario = "changed-pose-gradual";
    setup(r);
    request(r, 4, 0);
    CHECK(byte(r, camera, 0x211) == 4 && byte(r, camera, 0x212) == 1);
    CHECK(byte(r, camera, 0x213) == 0 && spies.event == 1 && spies.event_value == 4);

    scenario = "changed-parameters";
    setup(r);
    for (unsigned i = 0; i < 4; ++i) {
        put(r, camera + i * 4, bits(0));
        put(r, parameters + i * 4, bits(0));
    }
    auto c = context();
    c.r5 = 0;
    c.r6 = guest(parameters);
    ncpoddialog_entrypoint_4(r, &c);
    CHECK(byte(r, camera, 0x216) == 0);
    put(r, parameters, bits(30));
    c = context();
    c.r5 = 0;
    c.r6 = guest(parameters);
    ncpoddialog_entrypoint_4(r, &c);
    CHECK(byte(r, camera, 0x216) == 1);
    request(r, 3, 1);
    CHECK(byte(r, camera, 0x216) == 0 && byte(r, camera, 0x212) == 1);
    CHECK(byte(r, camera, 0x213) == 1 && spies.event == 1 && spies.event_value == 0x10003);

    scenario = "special-pose";
    setup(r);
    spies.request_result = 3;
    request(r, 7, 0);
    CHECK(spies.request == 1 && spies.stage == 1 && spies.event == 1);
    CHECK(byte(r, camera, 0x212) == 0);

    scenario = "completed";
    setup(r);
    set_byte(r, camera, 0x213, 1);
    put(r, camera + 0x20C, bits(1.25f));
    c = context();
    func_808015CC_ncpoddialog(r, &c);
    CHECK(near(number(get(r, camera + 0x20C)), 1));
    CHECK(spies.request == 1 && spies.advance == 0 && spies.evaluate == 0 && spies.submit == 0);
    CHECK(c.r2 == 4 && byte(r, camera, 0x213) == 1);
}
static void decode_cases(uint8_t* r) {
    scenario = "encoded-request";
    const std::array<std::array<uint32_t, 3>, 6> cases = {{{0, 0, 0}, {49, 49, 0},
        {50, 0, 50}, {99, 49, 50}, {100, 0, 1}, {107, 7, 1}}};
    for (const auto& row : cases) {
        auto c = context();
        c.r4 = row[0];
        c.r5 = guest(parameters + 0x100);
        c.r6 = guest(parameters + 0x104);
        func_80800000_gcdialogcamera(r, &c);
        CHECK(get(r, parameters + 0x100) == row[1]);
        CHECK(get(r, parameters + 0x104) == row[2]);
    }
}
int main() {
    std::vector<uint32_t> storage(0x800000U / sizeof(uint32_t));
    auto* r = reinterpret_cast<uint8_t*>(storage.data());
    decode_cases(r);
    motion(r, 0, 0.25f, "gradual-0");
    motion(r, 2, 0.75f, "gradual-2");
    motion(r, 1, 0.25f, "instant");
    request_cases(r);
    std::puts("PASS: actual dialogue update submits spline positions X=22.5/67.5 for controlled gradual inputs and X=90 for instant kind 1.");
    std::puts("PASS: same-pose no-op, pose change, unchanged/changed parameters, special-pose dispatch and completed-state controls passed.");
    std::puts("PASS: encoded request boundaries retain raw modifiers 0/50/1; modifier 50 motion is outside this probe's scope.");
}
