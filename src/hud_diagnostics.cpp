#include "tooie_recomp.h"
#include "runtime_diagnostics.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

namespace {
constexpr uint32_t Table = 0x80123880U, Count = 48, Stride = 0x1C;
constexpr uint64_t Period = 5'000'000;
constexpr unsigned MaxDepth = 8, MaxCommands = 128;
constexpr const char *Request = "hud-trace-v1";
constexpr const char *Contents = "tooie-hud-trace-v1\n";

bool valid(uint32_t address, uint32_t bytes) {
    return address >= 0x80000000U && uint64_t(address) + bytes <= 0x80800000ULL && !(address & 3U);
}
uint32_t word(const uint8_t *ram, uint32_t address) {
    uint32_t value = 0;
    if (ram && valid(address, 4)) std::memcpy(&value, ram + (address & 0x7FFFFFU), 4);
    return value;
}
int16_t half(const uint8_t *ram, uint32_t address) {
    uint16_t value = 0;
    if (ram && !(address & 1U) && address >= 0x80000000U && address <= 0x807FFFFEU)
        std::memcpy(&value, ram + ((address & 0x7FFFFFU) ^ 2U), 2);
    return static_cast<int16_t>(value);
}
bool requested() {
#if defined(__ANDROID__)
    const char *folder = std::getenv("APP_FOLDER_PATH");
    if (!folder || !folder[0]) return false;
    char path[1024];
    const int length = folder ? std::snprintf(path, sizeof(path), "%s/%s", folder, Request) : -1;
    if (length <= 0 || size_t(length) >= sizeof(path)) return false;
    FILE *file = std::fopen(path, "rb");
    if (!file) return false;
    char bytes[32];
    const size_t size = std::fread(bytes, 1, sizeof(bytes), file);
    const bool result = !std::ferror(file) && size == std::strlen(Contents) &&
        std::memcmp(bytes, Contents, size) == 0;
    std::fclose(file);
    return result;
#else
    return false;
#endif
}

struct Scope {
    bool captured = false, callbackObserved = false;
    uint32_t index = 0, component = 0, cursor = 0, start = 0, callback = 0;
    int32_t x = 0, y = 0, callbackX = 0, callbackY = 0;
    uint64_t sample = 0, draw = 0;
};
struct Observer {
    std::array<Scope, MaxDepth> scopes{};
    uint64_t depth = 0, sample = 0, seen = 0, nextDraw = 0;
    bool initialized = false, enabled = false;

    // This is an explicitly requested, read-only observer. The live table is
    // copied at the real HUD call boundary, not guessed from a glyph's X value.
    void layout(const uint8_t *ram) {
        const auto start = tooie::diagnostics::monotonicUs();
        char line[1100];
        for (uint32_t i = 0; i < Count; ++i) {
            const auto entry = Table + i * Stride;
            const auto component = word(ram, entry + 0x18);
            const bool readable = ram && valid(component, 0x34);
            char tableWords[80]{}, componentWords[160]{};
            for (unsigned j = 0; j < 7; ++j)
                std::snprintf(tableWords + j * 9, sizeof(tableWords) - j * 9,
                    "%08X%s", word(ram, entry + j * 4), j == 6 ? "" : "/");
            for (unsigned j = 0; j < 13; ++j)
                std::snprintf(componentWords + j * 9, sizeof(componentWords) - j * 9,
                    "%08X%s", readable ? word(ram, component + j * 4) : 0, j == 12 ? "" : "/");
            std::snprintf(line, sizeof(line),
                "sample=%llu index=%u entry=0x%X authored_xy=%d/%d draw_xy=%d/%d component=0x%X component_readable=%u entry_words=%s component_words=%s",
                (unsigned long long)sample, i, entry, int(half(ram, entry)), int(half(ram, entry + 2)), int(half(ram, entry + 6)), int(half(ram, entry + 8)),
                component, unsigned(readable), tableWords, componentWords);
            tooie::diagnostics::log("hud-layout", line);
        }
        std::snprintf(line, sizeof(line),
            "sample=%llu stage=layout-budget entries=%u observer_us=%llu scope=read-only-guest-inputs-excludes-this-log",
            (unsigned long long)sample, Count,
            (unsigned long long)(tooie::diagnostics::monotonicUs() - start));
        tooie::diagnostics::log("hud-layout", line);
    }
    void begin(const uint8_t *ram, const recomp_context *ctx, uint64_t now) {
        const uint64_t slot = depth++;
        if (slot >= scopes.size()) return;
        scopes[slot] = {};
        if (!initialized || now < sample || now - sample >= Period) {
            initialized = true; sample = now; seen = 0; enabled = requested();
            if (enabled && ram) layout(ram);
        }
        if (!enabled || !ram || !ctx) return;
        // func_800FA508 keeps the actual 0x1C-byte table entry in s0. a1
        // holds its component; a2 has already loaded +6. +8 is the jal's
        // delay-slot argument and is observed again at the real callback.
        const auto entry = uint32_t(ctx->r16), component = uint32_t(ctx->r5);
        if (entry < Table || entry >= Table + Count * Stride || (entry - Table) % Stride ||
            !component || word(ram, entry + 0x18) != component || !valid(component, 0x34)) return;
        const unsigned index = (entry - Table) / Stride;
        if (seen & (uint64_t(1) << index)) return;
        seen |= uint64_t(1) << index;
        auto &scope = scopes[slot];
        scope.captured = true; scope.sample = sample; scope.draw = ++nextDraw;
        scope.index = index; scope.component = component; scope.cursor = uint32_t(ctx->r4);
        scope.start = word(ram, scope.cursor);
        scope.x = half(ram, entry + 6); scope.y = half(ram, entry + 8);
    }
    void callback(const recomp_context *ctx) {
        if (!ctx || !depth || depth > scopes.size()) return;
        auto &scope = scopes[depth - 1];
        if (!scope.captured || uint32_t(ctx->r16) != scope.component) return;
        scope.callbackObserved = true; scope.callback = uint32_t(ctx->r2);
        scope.callbackX = int32_t(ctx->r18); scope.callbackY = int32_t(ctx->r19);
    }
    void end(const uint8_t *ram) {
        if (!depth) return;
        const uint64_t slot = --depth;
        if (slot >= scopes.size()) return;
        const auto scope = scopes[slot]; scopes[slot] = {};
        if (!scope.captured) return;
        const auto start = tooie::diagnostics::monotonicUs();
        const auto end = word(ram, scope.cursor);
        const bool range = ram && valid(scope.cursor, 4) && end >= scope.start &&
            !(scope.start & 7U) && !((end - scope.start) & 7U) && valid(scope.start, end - scope.start);
        const unsigned count = range ? (end - scope.start) / 8 : 0;
        const unsigned copied = std::min(count, MaxCommands);
        char line[1000];
        // Bound the copied command span. Nested display lists are addresses,
        // not recursively traversed here; commands are never submitted/edited.
        for (unsigned first = 0; first < copied; first += 8) {
            char commands[180]{};
            const unsigned size = std::min(8U, copied - first);
            for (unsigned j = 0; j < size; ++j) {
                const auto address = scope.start + (first + j) * 8;
                std::snprintf(commands + j * 18, sizeof(commands) - j * 18,
                    "%08X/%08X%s", word(ram, address), word(ram, address + 4), j + 1 == size ? "" : ";");
            }
            std::snprintf(line, sizeof(line),
                "sample=%llu draw=%llu index=%u stage=commands first=%u count=%u address=0x%X words=%s",
                (unsigned long long)scope.sample, (unsigned long long)scope.draw, scope.index,
                first, size, scope.start + first * 8, commands);
            tooie::diagnostics::log("hud-draw", line);
        }
        std::snprintf(line, sizeof(line),
            "sample=%llu draw=%llu index=%u stage=complete component=0x%X draw_xy=%d/%d callback_observed=%u callback=0x%X callback_xy=%d/%d cursor=0x%X begin=0x%X end=0x%X range_valid=%u commands=%u copied=%u omitted=%u observer_us=%llu scope=whole-component-including-related-text-cpu-only",
            (unsigned long long)scope.sample, (unsigned long long)scope.draw, scope.index, scope.component,
            scope.x, scope.y, unsigned(scope.callbackObserved), scope.callback, scope.callbackX, scope.callbackY,
            scope.cursor, scope.start, end, unsigned(range), count, copied, count - copied,
            (unsigned long long)(tooie::diagnostics::monotonicUs() - start));
        tooie::diagnostics::log("hud-draw", line);
    }
};
thread_local Observer observer;
}

extern "C" void tooie_hud_component_begin(const uint8_t *ram, const recomp_context *ctx) {
    observer.begin(ram, ctx, tooie::diagnostics::monotonicUs());
}
extern "C" void tooie_hud_component_callback(const uint8_t*, const recomp_context *ctx) {
    observer.callback(ctx);
}
extern "C" void tooie_hud_component_end(const uint8_t *ram, const recomp_context*) {
    observer.end(ram);
}
