// A measurement of production camera interpolation, not an MD-004 replay.
// Reuse the existing controlled Workload/DOM-free projection fixture.
#define main unused_dialogue_probe_main
#include "dialogue_projection_probe.cpp"
#undef main

struct TurnResult { float xLength, yLength, zLength, focalX; };

static hlslpp::float4x4 yawView(float degrees) {
    const float radians = degrees * 3.14159265358979323846f / 180.0f;
    auto result = hlslpp::float4x4::identity();
    result[0][0] = result[2][2] = std::cos(radians);
    result[0][2] = -std::sin(radians);
    result[2][0] = std::sin(radians);
    return result;
}

static TurnResult turn(float previousYaw, float currentYaw, float weight,
                       uint32_t flags, float aspect) {
    WorkloadQueue queue{};
    fill(queue.workloads[0], 0, kGameplayProjectionId, flags, false, {});
    fill(queue.workloads[1], 0, kGameplayProjectionId, flags, false, {});
    for (unsigned i = 0; i < 2; ++i) {
        auto &data = queue.workloads[i].drawData;
        data.viewTransforms[1] = yawView(i ? currentYaw : previousYaw);
        data.viewProjTransforms[1] = hlslpp::mul(data.viewTransforms[1], data.projTransforms[1]);
    }
    GameFrame previous, current;
    previous.workloads = {0}; current.workloads = {1};
    current.frameMap.workloads.resize(4);
    previous.perspectiveScenes = {GameScene{{{0, 0, 0}}}};
    current.perspectiveScenes = {GameScene{{{1, 0, 0}}}};
    bool velocity = false, tiles = false, lookAt = false;
    current.match(nullptr, queue, previous, nullptr, velocity, tiles, lookAt);
    REQUIRE(!velocity && !tiles && !lookAt);
    REQUIRE(current.frameMap.workloads[1].viewProjections[1].mapped);
    ProjectionProcessor consumer;
    ProjectionProcessor::ProcessParams params;
    params.workloadQueue = &queue; params.prevFrame = &previous; params.curFrame = &current;
    params.curFrameWeight = weight; params.aspectRatioScale = aspect;
    const uint32_t bypasses = g_bt_rt64_projection_viewport_bypasses.load();
    consumer.process(params);
    REQUIRE(g_bt_rt64_projection_viewport_bypasses.load() == bypasses);
    const auto &data = queue.workloads[1].drawData;
    const auto &view = data.modViewTransforms[1];
    const auto axisLength = [&](unsigned row) {
        return std::sqrt(view[row][0]*view[row][0] + view[row][1]*view[row][1] + view[row][2]*view[row][2]);
    };
    TurnResult result{axisLength(0), axisLength(1), axisLength(2), data.modProjTransforms[1][0][0]};
    REQUIRE(std::isfinite(result.xLength) && std::isfinite(result.zLength));
    REQUIRE(near(result.yLength, 1));
    REQUIRE(near(result.focalX, 1 / aspect));
    return result;
}

int main() {
    unsigned cases = 0;
    for (const float aspect : {1.0f, 4.0f / 3.0f}) {
        for (const float degrees : {0.0f, 3.0f, 10.0f, 30.0f, 60.0f, 90.0f}) {
            for (const float weight : {0.0f, .25f, .5f, .75f, 1.0f}) {
                const auto actual = turn(0, degrees, weight, kProjectionGroupFlags, aspect);
                const float radians = degrees * 3.14159265358979323846f / 180.0f;
                const float expected = std::sqrt(1 - 2 * weight * (1 - weight) * (1 - std::cos(radians)));
                REQUIRE(near(actual.xLength, expected) && near(actual.zLength, expected));
                // Opposing controls: a rigid camera blend and a submitted-only
                // camera both retain unit axes. These are fixture inputs, not
                // proposed production flags or a confirmed gameplay correction.
                const auto rigid = turn(0, degrees, weight, kProjectionGroupFlags | (1U << 2), aspect);
                const auto submitted = turn(0, degrees, weight, kProjectionGroupFlags & ~0x78U, aspect);
                REQUIRE(near(rigid.xLength, 1) && near(rigid.zLength, 1));
                REQUIRE(near(submitted.xLength, 1) && near(submitted.zLength, 1));
                if (degrees >= 10 && weight > 0 && weight < 1) {
                    REQUIRE(actual.xLength < rigid.xLength - .001f);
                    REQUIRE(actual.xLength < submitted.xLength - .001f);
                }
                cases += 3;
                if (weight == .5f) {
                    std::printf("{\"turn_degrees\":%.1f,\"aspect\":%.6f,\"weight\":%.2f,\"simple_axis_length\":%.8f,\"rigid_axis_length\":%.8f,\"submitted_axis_length\":%.8f}\n",
                        degrees, aspect, weight, actual.xLength, rigid.xLength, submitted.xLength);
                }
            }
        }
    }
    for (const float aspect : {1.0f, 4.0f / 3.0f}) {
        const auto wrap = turn(179, -179, .5f, kProjectionGroupFlags, aspect);
        REQUIRE(near(wrap.xLength, std::cos(3.14159265358979323846f / 180.0f)));
        ++cases;
    }
    std::printf("PASS: %u production CPU camera-turn measurements/controls. Synthetic rotations; no captured collision, GPU execution or MD-004 fix is established.\n", cases);
    return 0;
}
