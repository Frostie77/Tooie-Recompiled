// Compile the production observer in this translation unit so its clock window
// can also be exercised without waiting in real time. Guest consumers are real
// retained generated C; only external drawing/preparation services are fixtures.
#include "../src/hud_diagnostics.cpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "hud diagnostic failure line=%d %s\n", __LINE__, #x); std::exit(61); } } while (0)
extern "C" {
void func_800FA508(uint8_t*, recomp_context*);
void parent_func_800FA508(uint8_t*, recomp_context*);
void scinfobar_entrypoint_24(uint8_t*, recomp_context*);
void parent_scinfobar_entrypoint_24(uint8_t*, recomp_context*);
}
static int32_t addresses[855]{};
int32_t *section_addresses = addresses;
static constexpr uint32_t Cursor = 0x80500000U, Components = 0x80510000U, List = 0x80580000U;
static bool parentRun = false, specialOnly = false, nested = false, insideNested = false;
static unsigned icons = 0, texts = 0;
static std::filesystem::path folder;

static void put(uint8_t *ram, uint32_t address, uint32_t value) {
    CHECK(valid(address, 4)); std::memcpy(ram + (address & 0x7FFFFFU), &value, 4);
}
static void putHalf(uint8_t *ram, uint32_t address, int16_t value) {
    CHECK(address >= 0x80000000U && address <= 0x807FFFFEU && !(address & 1));
    std::memcpy(ram + ((address & 0x7FFFFFU) ^ 2), &value, 2);
}
static void putByte(uint8_t *ram, uint32_t address, uint8_t value) {
    ram[(address & 0x7FFFFFU) ^ 3U] = value;
}
static void append(uint8_t *ram, uint32_t cursor, uint32_t a, uint32_t b) {
    const auto at = word(ram, cursor); put(ram, at, a); put(ram, at + 4, b); put(ram, cursor, at + 8);
}
extern "C" void func_800A9C98(uint8_t*, recomp_context *ctx) { ctx->r2 = specialOnly; }
extern "C" void func_800DB9B0(uint8_t*, recomp_context *ctx) { ctx->r2 = 0; }
extern "C" void func_808006EC_scinfobar(uint8_t*, recomp_context*) {}
extern "C" void func_80800608_scinfobar(uint8_t *ram, recomp_context *ctx) {
    append(ram, uint32_t(ctx->r4), 0xE4000000U | ++texts, uint32_t(ctx->r5));
}
extern "C" void _scinfobar_entrypoint_24(uint8_t *ram, recomp_context *ctx) {
    (parentRun ? parent_scinfobar_entrypoint_24 : scinfobar_entrypoint_24)(ram, ctx);
}
static void draw(uint8_t *ram, recomp_context *ctx) {
    const auto cursor = uint32_t(ctx->r4);
    append(ram, cursor, 0xDE000000U | ++icons, uint32_t(ctx->r5));
    if (nested && !insideNested) {
        insideNested = true;
        ctx->r4 = int32_t(cursor);
        (parentRun ? parent_func_800FA508 : func_800FA508)(ram, ctx);
        insideNested = false;
    }
}
extern "C" recomp_func_t *tooie_lookup_func(int32_t) { return draw; }
extern "C" recomp_func_t *tooie_lookup_func_at(int32_t address, uint8_t*, recomp_context*, const char *caller, uint32_t line) {
    CHECK(uint32_t(address) == 0x80005000U && caller && line); return draw;
}

static std::string logText() {
    std::ifstream file(folder / "banjotooi-debug.log");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
static void clearLog() { std::filesystem::remove(folder / "banjotooi-debug.log"); }
static void request(const std::string &contents) {
    std::ofstream file(folder / Request, std::ios::binary); file << contents;
}
static unsigned occurrences(const std::string &s, const std::string &needle) {
    unsigned count = 0;
    for (size_t p = 0; (p = s.find(needle, p)) != std::string::npos; p += needle.size()) ++count;
    return count;
}
static recomp_context context() {
    recomp_context ctx{}; ctx.r29 = int32_t(0x807F0000U); ctx.r4 = int32_t(Cursor);
    ctx.r31 = int32_t(0x80001234U);
    ctx.r16 = 0xABC00016; ctx.r17 = 0xABC00017; ctx.r18 = 0xABC00018;
    return ctx;
}
static std::vector<uint8_t> memory(unsigned mode, bool sparse, bool visible) {
    std::vector<uint8_t> ram(0x800000);
    addresses[786] = int32_t(0x80600000U);
    put(ram.data(), Cursor, List); putByte(ram.data(), 0x801357B0, visible);
    for (unsigned i = 0; i < Count; ++i) {
        const auto entry = Table + i * Stride, component = Components + i * 0x80;
        putHalf(ram.data(), entry, 30); putHalf(ram.data(), entry + 2, 40);
        putHalf(ram.data(), entry + 6, int16_t((i % 3) * 148 + 12));
        putHalf(ram.data(), entry + 8, int16_t(24 + (i % 4) * 30));
        put(ram.data(), entry + 0x18, sparse && i != 0 && i != 7 ? 0 : component);
        putByte(ram.data(), component, mode == 1 ? 100 : mode == 2 ? 101 : 0);
        putByte(ram.data(), component + 2, mode == 2 ? 101 : 0);
        put(ram.data(), component + 4, 0x5000 + i);
        put(ram.data(), component + 0x14, 0x3F000000); // controlled fade
        put(ram.data(), component + 0x1C, i % 3);
        put(ram.data(), 0x80600D54 + (i % 3) * 12, mode == 3 ? 0 : 0x80005000);
    }
    return ram;
}
static void consumers(bool enabled, bool offOnly, unsigned mode, bool sparse, bool visible, bool special, bool recursion) {
    observer = {}; clearLog();
    if (enabled) request(Contents); else std::filesystem::remove(folder / Request);
    const auto initial = memory(mode, sparse, visible);
    auto baseline = initial, current = initial;
    auto expected = context(), actual = expected;
    specialOnly = special; nested = recursion;
    parentRun = true; insideNested = false; icons = texts = 0;
    parent_func_800FA508(baseline.data(), &expected);
    const auto oldIcons = icons, oldTexts = texts;
    CHECK(logText().empty());
    parentRun = false; insideNested = false; icons = texts = 0;
    func_800FA508(current.data(), &actual);
    CHECK(icons == oldIcons && texts == oldTexts);
    CHECK(baseline == current && std::memcmp(&actual, &expected, sizeof(actual)) == 0);
    CHECK(observer.depth == 0);
    const auto log = logText();
    unsigned count = visible ? special ? 3 : sparse ? 2 : Count : 0;
    if (!enabled || offOnly) count = 0;
    CHECK(occurrences(log, "stage=complete") == count);
    if (count) {
        CHECK(occurrences(log, "entry_words=") == Count);
        CHECK(log.find("index=0 entry=0x80123880 authored_xy=30/40 draw_xy=12/24") != std::string::npos);
        CHECK(log.find("range_valid=1") != std::string::npos);
        if (!recursion) {
            const unsigned commands = mode == 1 || mode == 3 ? 1 : mode == 2 ? 3 : 2;
            CHECK(occurrences(log, "commands=" + std::to_string(commands) + " copied=" + std::to_string(commands) + " omitted=0") == count);
        }
        CHECK(log.find(mode == 1 || mode == 3 ? "callback_observed=0" : "callback_observed=1") != std::string::npos);
        if (mode != 1 && mode != 3 && !special)
            CHECK(log.find("callback=0x80005000 callback_xy=12/24") != std::string::npos);
        CHECK(log.find("scope=whole-component-including-related-text-cpu-only") != std::string::npos);
    } else CHECK(log.empty());
}
static void windowsAndBounds() {
    auto ram = memory(0, false, true); auto ctx = context();
    ctx.r16 = int32_t(Table); ctx.r5 = int32_t(Components);
    Observer o;
    auto step = [&](uint64_t now) { o.begin(ram.data(), &ctx, now); o.end(ram.data()); CHECK(o.depth == 0); };
    clearLog(); std::filesystem::remove(folder / Request); step(1); CHECK(logText().empty());
    request("wrong\n"); step(5'000'001); CHECK(logText().empty());
    request(std::string(Contents) + '\0'); step(10'000'001); CHECK(logText().empty());
    request(Contents); step(15'000'001); CHECK(occurrences(logText(), "stage=complete") == 1);
    step(15'000'002); CHECK(occurrences(logText(), "stage=complete") == 1);
    step(20'000'001); CHECK(occurrences(logText(), "stage=complete") == 2);
    std::filesystem::remove(folder / Request); step(25'000'001); CHECK(occurrences(logText(), "stage=complete") == 2);
    request(Contents); step(1); CHECK(occurrences(logText(), "stage=complete") == 3);

    clearLog(); o = {}; put(ram.data(), Cursor, List);
    for (unsigned j = 0; j < MaxCommands + 9; ++j) { put(ram.data(), List + j * 8, 0xDE000000U | j); put(ram.data(), List + j * 8 + 4, j); }
    o.begin(ram.data(), &ctx, 1); put(ram.data(), Cursor, List + (MaxCommands + 9) * 8);
    const auto before = ram; const auto beforeCtx = ctx;
    o.end(ram.data()); CHECK(ram == before && std::memcmp(&beforeCtx, &ctx, sizeof(ctx)) == 0);
    CHECK(logText().find("commands=137 copied=128 omitted=9") != std::string::npos);

    for (uint32_t end : {List - 8, List + 3, 0x80800100U}) {
        clearLog(); o = {}; put(ram.data(), Cursor, List); o.begin(ram.data(), &ctx, 1); put(ram.data(), Cursor, end);
        o.end(ram.data()); CHECK(logText().find("range_valid=0 commands=0 copied=0 omitted=0") != std::string::npos);
    }
    clearLog(); o = {}; o.begin(nullptr, nullptr, 1); o.callback(nullptr); o.end(nullptr); CHECK(o.depth == 0 && logText().empty());
    // An invalid nested component cannot attribute its callback to its parent.
    o.begin(ram.data(), &ctx, 5'000'001);
    auto bad = ctx; bad.r16 = 0xFFFFFFFF; o.begin(ram.data(), &bad, 5'000'001);
    auto note = ctx; note.r16 = int32_t(Components); note.r2 = int32_t(0x80005000U);
    o.callback(&note); o.end(ram.data()); CHECK(o.depth == 1 && !o.scopes[0].callbackObserved);
    o.end(ram.data()); CHECK(o.depth == 0);
}
int main(int argc, char **argv) {
    CHECK(argc == 2 || argc == 3); folder = argv[1]; std::filesystem::create_directories(folder);
    CHECK(setenv("APP_FOLDER_PATH", folder.c_str(), 1) == 0);
    const bool offOnly = argc == 3;
    if (!offOnly) windowsAndBounds();
    unsigned cases = 0;
    for (bool enabled : {false, true}) for (unsigned mode : {0U, 1U, 2U, 3U}) {
        consumers(enabled, offOnly, mode, false, true, false, false); ++cases;
    }
    consumers(true, offOnly, 0, true, true, false, false); ++cases;
    consumers(true, offOnly, 0, false, false, false, false); ++cases;
    consumers(true, offOnly, 0, false, true, true, false); ++cases;
    consumers(true, offOnly, 0, false, true, false, true); ++cases;
    std::printf("PASS: %u complete HUD-consumer cases: guest RAM/ABI/Gfx unchanged, whole component/callback/text, authored positions, filtering, empty paths, nested scopes and default-Off%s\n", cases, offOnly ? " outside Android" : " with opt-in/removal, malformed request and bounded spans");
}
