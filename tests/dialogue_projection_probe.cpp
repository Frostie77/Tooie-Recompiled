#include "hle/rt64_game_frame.h"
#include "render/rt64_projection_processor.h"
#include "probe_queue.h"
#include "producer.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#define REQUIRE(x) do {if (!(x)) {std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}} while (0)
using namespace RT64;
extern "C" std::atomic<uint32_t> g_bt_rt64_proj_adjusted_other;
extern "C" std::atomic<uint32_t> g_bt_rt64_projection_viewport_bypasses;

// The CPU path must never create/upload GPU buffers in this fixture.
void BufferUploader::submit(RenderWorker *, const std::vector<Upload> &) { REQUIRE(false); }
BufferUploader::~BufferUploader() { REQUIRE(false); }

static TransformGroup group(uint32_t id, uint32_t flags) {
    TransformGroup g;
    g.matrixId = id;
    g.decompose = ((flags >> 2) & 1U) != 0;
    g.positionInterpolation = (flags >> 3) & 3U;
    g.rotationInterpolation = (flags >> 5) & 3U;
    g.scaleInterpolation = (flags >> 7) & 3U;
    g.skewInterpolation = (flags >> 9) & 3U;
    g.perspectiveInterpolation = (flags >> 11) & 3U;
    g.aspectMode = (flags >> 20) & 3U;
    return g;
}

struct ViewportInput {
    float halfWidth=160, halfHeight=120, centerY=120, depthScale=1;
    float focalY=4.0f/3.0f;
    uint16_t origin=G_EX_ORIGIN_NONE;
    int16_t clipTop=1;
    int32_t scissorTop=4;
};

static void fill(Workload &w, float x, uint32_t id, uint32_t flags, bool widerPair,
                 const ViewportInput &input) {
    auto &d = w.drawData;
    const interop::float4x4 view = matrixTranslation(hlslpp::float3(x, 0, 0));
    interop::float4x4 projection = hlslpp::float4x4::identity();
    projection[0][0] = 1.0f;
    projection[1][1] = input.focalY;
    projection[2][2] = -101.0f / 99.0f;
    projection[2][3] = -1.0f;
    projection[3][2] = -200.0f / 99.0f;
    projection[3][3] = 0.0f;
    d.viewTransforms = {view, view};
    d.projTransforms = {projection, projection};
    d.viewProjTransforms = {hlslpp::mul(view, projection), hlslpp::mul(view, projection)};
    d.transformGroups = {group(id, flags)};
    d.viewProjTransformGroups = {0, 0};
    interop::RSPViewport viewport{};
    viewport.scale = hlslpp::float3(input.halfWidth, input.halfHeight, input.depthScale);
    viewport.translate = hlslpp::float3(160, input.centerY, 0);
    d.rspViewports = {viewport, viewport};
    d.viewportClipRatios = {1, input.clipTop, -1, -1, 1, input.clipTop, -1, -1};
    d.viewportOrigins = {input.origin, input.origin};
    w.fbPairs.resize(1);
    w.fbPairCount = 1;
    auto &fb = w.fbPairs[0];
    fb.scissorRect = widerPair ? FixedRect(0, 0, 1280, 960) : FixedRect(4, 4, 1276, 956);
    fb.projections.resize(1);
    fb.projectionCount = 1;
    auto &p = fb.projections[0];
    p.type = Projection::Type::Perspective;
    p.transformsIndex = 1;
    p.gameCallCount = 0;
    p.scissorRect = FixedRect(4, input.scissorTop, 1276, 956);
}

struct Result {float viewX, focalX, focalY; bool mapped, adjusted, bypassed;};
static Result probe(const char *name, float currentX, float weight, bool widerPair = false,
                    uint32_t flags = kProjectionGroupFlags,
                    uint32_t id = kGameplayProjectionId,
                    float aspectRatioScale = 4.0f / 3.0f,
                    const ViewportInput &previousViewport = {},
                    const ViewportInput &currentViewport = {},
                    uint32_t previousId = kGameplayProjectionId) {
    WorkloadQueue q{};
    fill(q.workloads[0], 0, previousId, kProjectionGroupFlags, false, previousViewport);
    fill(q.workloads[1], currentX, id, flags, widerPair, currentViewport);
    GameFrame previous, current;
    previous.workloads = {0};
    current.workloads = {1};
    current.frameMap.workloads.resize(4);
    previous.perspectiveScenes = {GameScene{{{0, 0, 0}}}};
    current.perspectiveScenes = {GameScene{{{1, 0, 0}}}};
    bool velocity = false, tiles = false, lookAt = false;
    current.match(nullptr, q, previous, nullptr, velocity, tiles, lookAt);
    REQUIRE(!velocity && !tiles && !lookAt);
    ProjectionProcessor processor;
    ProjectionProcessor::ProcessParams params;
    params.workloadQueue = &q;
    params.prevFrame = &previous;
    params.curFrame = &current;
    params.curFrameWeight = weight;
    params.aspectRatioScale = aspectRatioScale;
    const uint32_t adjustedBefore = g_bt_rt64_proj_adjusted_other.load(std::memory_order_relaxed);
    const uint32_t bypassesBefore = g_bt_rt64_projection_viewport_bypasses.load(std::memory_order_relaxed);
    processor.process(params);
    const auto &d = q.workloads[1].drawData;
    Result result{d.modViewTransforms[1][3][0], d.modProjTransforms[1][0][0], d.modProjTransforms[1][1][1],
                  current.frameMap.workloads[1].viewProjections[1].mapped,
                  g_bt_rt64_proj_adjusted_other.load(std::memory_order_relaxed) != adjustedBefore,
                  g_bt_rt64_projection_viewport_bypasses.load(std::memory_order_relaxed) != bypassesBefore};
    REQUIRE(std::isfinite(result.viewX) && std::isfinite(result.focalX) && std::isfinite(result.focalY));
    std::printf("{\"case\":\"%s\",\"view_x\":%.6f,\"focal_x\":%.6f,\"mapped\":%s,\"aspect_adjusted\":%s}\n",
                name, result.viewX, result.focalX, result.mapped ? "true" : "false",
                result.adjusted ? "true" : "false");
    return result;
}

static bool near(float a, float b) {return std::fabs(a - b) < 0.0001f;}
int main() {
    static_assert(kProjectionGroupFlags == 0x02008AAAU);
    const auto smooth = probe("smooth_camera", 1, .25f);
    REQUIRE(smooth.mapped && near(smooth.viewX, .25f));
    const auto cut = probe("synthetic_cut_same_id", 100, .25f);
    REQUIRE(cut.mapped && near(cut.viewX, 25));
    REQUIRE(near(probe("current_endpoint", 100, 1).viewX, 100));
    const auto differentId = probe("different_id_control", 100, .25f, false,
                                   kProjectionGroupFlags, kGameplayProjectionId + 100);
    REQUIRE(!differentId.mapped && near(differentId.viewX, 100));
    const auto skip = probe("skip_camera_control", 100, .25f, false,
                            kProjectionGroupFlags & ~0x78U);
    REQUIRE(skip.mapped && near(skip.viewX, 100));
    REQUIRE(near(probe("covers_pair_width", 0, .25f).focalX, .75f));
    REQUIRE(near(probe("wider_pair_one_pixel_margin", 0, .25f, true).focalX, 1));
    const uint32_t forceAdjust = (kProjectionGroupFlags & ~(3U << 20)) | (G_EX_ASPECT_ADJUST << 20);
    REQUIRE(near(probe("explicit_aspect_control", 0, .25f, true, forceAdjust).focalX, .75f));
    // Original mode sets target aspect equal to source aspect. The consumer's
    // classification still changes, but reciprocal scale 1 cannot change focal X.
    const auto originalNarrow = probe("original_covers_pair_width", 0, .25f, false,
                                      kProjectionGroupFlags, kGameplayProjectionId, 1);
    const auto originalWide = probe("original_wider_pair", 0, .25f, true,
                                    kProjectionGroupFlags, kGameplayProjectionId, 1);
    REQUIRE(originalNarrow.adjusted && !originalWide.adjusted);
    REQUIRE(near(originalNarrow.focalX, 1) && near(originalWide.focalX, 1));
    const auto originalCut = probe("original_same_role_cut", 100, .25f, false,
                                   kProjectionGroupFlags, kGameplayProjectionId, 1);
    REQUIRE(originalCut.mapped && near(originalCut.viewX, 25) && near(originalCut.focalX, 1));
    ViewportInput full, letterbox;
    letterbox.halfHeight = 90;
    letterbox.focalY = 2.5f;
    // Actual matcher and consumer: changed viewport uses the submitted camera
    // and projection together; stable viewports still interpolate normally.
    for (const float aspect : {1.0f, 4.0f/3.0f}) {
        const auto enter = probe("enter_letterbox", 100, .25f, false,
            kProjectionGroupFlags, kGameplayProjectionId, aspect, full, letterbox);
        REQUIRE(enter.mapped && enter.bypassed && near(enter.viewX, 100) && near(enter.focalY, 2.5f));
        const auto exit = probe("exit_letterbox", 100, .75f, false,
            kProjectionGroupFlags, kGameplayProjectionId, aspect, letterbox, full);
        REQUIRE(exit.mapped && exit.bypassed && near(exit.viewX, 100) && near(exit.focalY, 4.0f/3.0f));
        const auto stable = probe("stable_letterbox", 100, .25f, false,
            kProjectionGroupFlags, kGameplayProjectionId, aspect, letterbox, letterbox);
        REQUIRE(stable.mapped && !stable.bypassed && near(stable.viewX, 25) && near(stable.focalY, 2.5f));
    }
    auto changedViewport = [&](const char *name, const ViewportInput &input) {
        const auto result = probe(name, 100, .25f, false,
            kProjectionGroupFlags, kGameplayProjectionId, 1, full, input);
        REQUIRE(result.mapped && result.bypassed && near(result.viewX, 100));
    };
    ViewportInput width=full, offset=full, depth=full, origin=full, clip=full;
    width.halfWidth=150; offset.centerY=100; depth.depthScale=2;
    origin.origin=G_EX_ORIGIN_CENTER; clip.clipTop=2;
    changedViewport("viewport_width_change", width);
    changedViewport("viewport_center_change", offset);
    changedViewport("viewport_depth_change", depth);
    changedViewport("viewport_origin_change", origin);
    changedViewport("viewport_clip_change", clip);
    ViewportInput fovOnly=full, scissorOnly=full;
    fovOnly.focalY=2.5f; scissorOnly.scissorTop=100;
    const auto fov = probe("stable_viewport_fov_animation", 100, .25f, false,
        kProjectionGroupFlags, kGameplayProjectionId, 1, full, fovOnly);
    REQUIRE(!fov.bypassed && near(fov.viewX, 25) && near(fov.focalY, 1.625f));
    const auto scissor = probe("draw_scissor_only_control", 100, .25f, false,
        kProjectionGroupFlags, kGameplayProjectionId, 1, full, scissorOnly);
    REQUIRE(!scissor.bypassed && near(scissor.viewX, 25));
    const auto otherRole = probe("non_gameplay_role_control", 100, .25f, false,
        kProjectionGroupFlags, kGameplayProjectionId + 1, 1, full, letterbox,
        kGameplayProjectionId + 1);
    REQUIRE(otherRole.mapped && !otherRole.bypassed && near(otherRole.viewX, 25));
    std::puts("PASS: viewport entry/exit use current camera/projection; stable letterbox, FOV-only animation and draw-scissor controls retain blending in production CPU consumers. MD-002 needs device verification.");
    return 0;
}
