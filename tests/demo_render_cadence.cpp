// Native command emission and actual RT64 decode/tick expressions, guarded RAM.
// This executes no graphics driver and makes no device performance claim.
#include "consumer.inc"
#include <cstdio>
#include <thread>
#include <vector>

static unsigned cases = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "render-cadence failure: %s at %d\n", #x, __LINE__); return 1; } } while (0)
static constexpr uint32_t data = 0x80300000U, stack = 0x807FF000U;
static void B(uint8_t *ram, uint32_t a, uint8_t v) { ram[(a & 0x7FFFFFU) ^ 3U] = v; }
static void H(uint8_t *ram, uint32_t a, uint16_t v) { std::memcpy(ram + ((a & 0x7FFFFFU) ^ 2U), &v, 2); }
static void accept(uint8_t *ram, recomp_context &ctx, unsigned span) {
    tooie::demo::pacing() = {};
    ctx = {}; ctx.r2 = 1; ctx.r29 = int32_t(stack);
    B(ram, 0x80127760U, 2); H(ram, 0x80127766U, 335);
    write_word(ram, 0x8012D5B0U, data); write_word(ram, 0x8012D5B8U, 1);
    write_word(ram, 0x8012D5BCU, 4); B(ram, data + 4, span);
    write_word(ram, stack + 0x50, span);
    tooie::demo::event(ram, &ctx, 0);
}
static uint32_t render(uint8_t *ram, unsigned divisor) {
    write_word(ram, kViRetraceInterval, divisor);
    write_word(ram, kRenderBufferState, 0x80400000U);
    failures = 0; g_frame_enables = 0;
    native_commands(ram);
    DisplayList command{{read_word(ram, 0x80400008U), read_word(ram, 0x8040000CU)}};
    DisplayList *cursor = &command; State state;
    setRefreshRateV1(&state, &cursor);
    Workload workload{}; copy_rate(state.extended, workload);
    return workload.viOriginalRate;
}
int main() {
    std::vector<uint8_t> memory(0x800000), other(0x800000);
    auto *ram = memory.data(); recomp_context ctx{};
    for (unsigned divisor : {2U, 1U, 3U}) for (unsigned span : {3U, 1U, 2U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U}) {
        accept(ram, ctx, span);
        const auto context = ctx;
        const auto record = tooie::demo::pacing().pending;
        const auto rate = render(ram, divisor);
#if defined(__ANDROID__)
        const unsigned expected = std::max(divisor, span);
#else
        const unsigned expected = divisor;
#endif
        const auto planned = planned_frames(rate, 120);
        if (rate != 60 / expected) {
            std::fprintf(stderr, "render-cadence mismatch: configured=%u record=%u wait=%u emitted=%u planned120=%u\n", divisor, span, expected, rate, planned);
            return 1;
        }
        CHECK(planned == 120 / rate);
        if (expected == 3) CHECK(planned == 6);
        CHECK(!failures && g_frame_enables == 1);
        CHECK(read_word(ram, 0x80400000U) == kEnableWord0);
        CHECK(read_word(ram, 0x80400004U) == kEnableWord1);
        CHECK(read_word(ram, 0x80400008U) == kSetRefreshRateWord0);
        CHECK(read_word(ram, kRenderBufferState) == 0x80400010U);
        CHECK(!std::memcmp(&context, &ctx, sizeof(ctx)));
        CHECK(tooie::demo::pacing().pending.owner == record.owner);
        CHECK(tooie::demo::pacing().pending.span == record.span);
        // Rendering must not consume the record subsequently used by the wait.
        tooie::demo::event(ram, &ctx, 1); ctx.r2 = divisor;
        tooie::demo::event(ram, &ctx, 2);
        CHECK(unsigned(ctx.r2) == expected);
        CHECK(read_word(ram, kViRetraceInterval) == divisor);
        tooie::demo::event(ram, &ctx, 3);
        CHECK(render(ram, divisor) == 60 / divisor); ++cases;
    }
    // Every component of the owned record participates in invalidation.
    for (unsigned mutation = 0; mutation < 8; ++mutation) {
        accept(ram, ctx, 3);
        switch (mutation) {
        case 0: B(ram, 0x80127760U, 1); break;
        case 1: H(ram, 0x80127766U, 278); break;
        case 2: write_word(ram, 0x8012D5B0U, data + 6); break;
        case 3: write_word(ram, 0x8012D5B8U, 2); break;
        case 4: write_word(ram, 0x8012D5BCU, 5); break;
        case 5: ctx.r2 = 0; tooie::demo::event(ram, &ctx, 0); break;
        case 6: write_word(ram, stack + 0x50, 7); tooie::demo::event(ram, &ctx, 0); break;
        case 7: other = memory; ram = other.data(); break;
        }
        CHECK(render(ram, 2) == 30); ram = memory.data(); ++cases;
    }
    accept(ram, ctx, 3);
    for (unsigned divisor : {0U, 61U, 0xFFFFFFFFU}) { CHECK(render(ram, divisor) == 30); ++cases; }
    uint32_t threaded = 0;
    std::thread thread([&] { threaded = render(ram, 2); }); thread.join();
    CHECK(threaded == 30); ++cases;
    tooie::demo::pacing() = {};
    for (unsigned divisor : {1U, 2U, 3U}) { CHECK(render(ram, divisor) == 60 / divisor); ++cases; }
    failures = 0; write_word(ram, kRenderBufferState, 0);
    native_commands(ram); CHECK(failures == 1); ++cases;
    const auto before = memory;
    CHECK(!append_command(ram, 0, 1, 2) && memory == before); ++cases;
    std::printf("PASS: %u native render-cadence cases; command bytes, RT64 rate/schedule, wait ownership, ordinary/desktop fallback and invalidation\n", cases);
}
