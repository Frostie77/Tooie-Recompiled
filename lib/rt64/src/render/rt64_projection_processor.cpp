//
// RT64
//

#include "rt64_projection_processor.h"

#include "common/rt64_math.h"
#include "rt64_camera_diagnostics.h"
#include "hle/rt64_workload_queue.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// TEMPORARY diagnostic (Banjo-Tooie port): how RT64 classified each projection
// for the widescreen aspect adjustment, bucketed by kind. This is the decision
// that decides whether a projection is widened, and it is taken per projection
// from that projection's own scissor/viewport -- so a projection that draws with
// a different viewport (Tooie's sky draw does) can classify differently from the
// scene's main camera and end up with a different field of view.
extern "C" {
std::atomic<uint32_t> g_bt_rt64_proj_adjusted_sky{0};
std::atomic<uint32_t> g_bt_rt64_proj_adjusted_other{0};
std::atomic<uint32_t> g_bt_rt64_proj_skipped_sky{0};
std::atomic<uint32_t> g_bt_rt64_proj_skipped_other{0};
extern std::atomic<uint32_t> g_bt_rt64_projection_viewport_bypasses;
}

namespace RT64 {
    // TEMPORARY diagnostic (Banjo-Tooie port). TOOIE_TRACE_PROJ=1 dumps, per
    // projection per frame, the matrices RT64 renders with and how it derived
    // them. RT64 replaces the game's merged matrix with mul(viewEstimate,
    // projEstimate) whenever processProjections is on (any aspect-ratio or
    // refresh-rate setting other than Original), so if the RSP's view/proj split
    // does not reproduce the matrix the game loaded, the render silently differs
    // from the game's own. Off by default; inert when unset.
    int traceProjMode() {
        static const int mode = [] {
            const char *value = std::getenv("TOOIE_TRACE_PROJ");
            return (value != nullptr) ? std::atoi(value) : 0;
        }();
        return mode;
    }

    bool traceProjEnabled() {
        return traceProjMode() >= 1;
    }

    static void traceProjPrintMatrix(const char *label, const interop::float4x4 &m) {
        std::fprintf(stderr,
            "       %s [%10.4f %10.4f %10.4f %10.4f] [%10.4f %10.4f %10.4f %10.4f] [%10.4f %10.4f %10.4f %10.4f] [%10.4f %10.4f %10.4f %10.4f]\n",
            label,
            m[0][0], m[0][1], m[0][2], m[0][3],
            m[1][0], m[1][1], m[1][2], m[1][3],
            m[2][0], m[2][1], m[2][2], m[2][3],
            m[3][0], m[3][1], m[3][2], m[3][3]);
    }

    uint32_t traceProjFrame = 0;
    static uint32_t traceMatrixDumps = 0;

    inline void adjustProjectionMatrix(interop::float4x4 &matrix, const float aspectRatioScale) {
        matrix[0][0] *= aspectRatioScale;
        matrix[1][0] *= aspectRatioScale;
        matrix[2][0] *= aspectRatioScale;
        matrix[3][0] *= aspectRatioScale;
    }

    // Tooie uses the standard infinite-far-plane form for some of its
    // projections (in first person, in sustained stretches): m[2][2] == m[2][3]
    // == -1.0, which makes the far plane genuinely infinite. nearPlaneFromProj /
    // farPlaneFromProj cannot express that -- far becomes (m32 - m33) / 0, i.e.
    // -inf, which their std::max clamp reports as 0.000 -- so the trace prints
    // this flag and the raw depth row instead of relying on those two numbers.
    static bool isInfiniteFarPlane(const interop::float4x4 &m) {
        return m[2][2] == m[2][3];
    }

    static bool skyInfiniteEnabled() {
        static const bool enabled = [] {
            const char* value = std::getenv("BT_RT64_SKY_INFINITE");
            return (value != nullptr) && ((value[0] == 'o') || (value[0] == '1'));
        }();
        return enabled;
    }

    // TEMPORARY diagnostic (Banjo-Tooie port). BT_RT64_SKY_INFINITE=on rewrites
    // the skybox projection's depth row into the infinite-far-plane form Tooie
    // uses in first person, keeping its near plane. That form is reachable in the
    // attract loop (whose sky otherwise uses a modest finite far plane), so this
    // makes the first-person projection state reproducible locally -- which is
    // what is needed to test whether the projection's near/far is what breaks the
    // sky, without a controller.
    static void makeInfiniteFarPlane(interop::float4x4 &matrix) {
        const float nearPlane = nearPlaneFromProj(matrix);
        matrix[2][2] = -1.0f;
        matrix[2][3] = -1.0f;
        matrix[3][2] = -2.0f * nearPlane;
        matrix[3][3] = 0.0f;
    }
    
    // ProjectionProcessor

    ProjectionProcessor::ProjectionProcessor() { }

    ProjectionProcessor::~ProjectionProcessor() {
        bufferUploader.reset(nullptr);
    }

    void ProjectionProcessor::setup(RenderWorker *worker) {
        bufferUploader = std::make_unique<BufferUploader>(worker->device);
    }

    void ProjectionProcessor::process(const ProcessParams &p) {
        traceProjFrame++; // TEMPORARY diagnostic
        for (uint32_t w : p.curFrame->workloads) {
            Workload &workload = p.workloadQueue->workloads[w];
            DrawData &drawData = workload.drawData;

            // Copy the data.
            drawData.modViewTransforms = drawData.viewTransforms;
            drawData.modProjTransforms = drawData.projTransforms;
            drawData.modViewProjTransforms = drawData.viewProjTransforms;
            drawData.prevViewTransforms = drawData.viewTransforms;
            drawData.prevProjTransforms = drawData.projTransforms;
            drawData.prevViewProjTransforms = drawData.viewProjTransforms;
        }

        for (size_t s = 0; s < p.curFrame->perspectiveScenes.size(); s++) {
            processScene(p, p.curFrame->perspectiveScenes[s], s);
        }

        for (size_t s = 0; s < p.curFrame->orthographicScenes.size(); s++) {
            processScene(p, p.curFrame->orthographicScenes[s], s);
        }
    }

    // A view matrix and the viewport it was submitted for form one mapping.
    // Reusing the previous camera with the current letterbox/viewport produces
    // a mixed mapping during a transition, even at Original aspect ratio.
    // Compare authored viewport state, not per-draw scissors (which vary with
    // geometry). Keep ordinary camera/FOV motion at a stable viewport smooth.
    static bool sameProjectionViewport(const DrawData &previous, uint32_t previousIndex,
                                      const DrawData &current, uint32_t currentIndex) {
        const auto &a = previous.rspViewports[previousIndex];
        const auto &b = current.rspViewports[currentIndex];
        for (uint32_t axis = 0; axis < 3; ++axis) {
            if ((a.scale[axis] != b.scale[axis]) || (a.translate[axis] != b.translate[axis])) {
                return false;
            }
        }
        if (previous.viewportOrigins[previousIndex] != current.viewportOrigins[currentIndex]) {
            return false;
        }
        for (uint32_t i = 0; i < 4; ++i) {
            if (previous.viewportClipRatios[previousIndex * 4 + i] != current.viewportClipRatios[currentIndex * 4 + i]) {
                return false;
            }
        }
        return true;
    }

    void ProjectionProcessor::processScene(const ProcessParams &p, const GameScene &scene, size_t sceneIndex) {
        for (size_t i = 0; i < scene.projections.size(); i++) {
            const GameIndices::Projection &sceneProj = scene.projections[i];
            Workload &workload = p.workloadQueue->workloads[sceneProj.workloadIndex];
            DrawData &drawData = workload.drawData;
            const FramebufferPair &fbPair = workload.fbPairs[sceneProj.fbPairIndex];
            const Projection &proj = fbPair.projections[sceneProj.projectionIndex];
            const uint16_t viewportOrigin = drawData.viewportOrigins[proj.transformsIndex];
            assert(proj.transformsIndex > 0);

            // Skip projections that didn't actually draw anything.
            if (proj.scissorRect.isNull()) {
                continue;
            }

            // Check the current mapping for the projection.
            const interop::float4x4 *prevProjMatrix = nullptr;
            const interop::float4x4 *prevViewMatrix = nullptr;
            const RigidBody *rigidBody = nullptr;
            // TEMPORARY diagnostics: what this projection was paired with.
            bool traceMapped = false;
            bool traceViewportBypass = false;
            const DrawData *tracePreviousData = nullptr;
            uint64_t tracePreviousWorkload = 0;
            int32_t tracePrevPairedTi = -1;
            uint32_t tracePrevPairedId = 0xFFFFFFFFU;
            const GameFrameMap::WorkloadMap &workloadMap = p.curFrame->frameMap.workloads[sceneProj.workloadIndex];
            if ((p.prevFrame != nullptr) && workloadMap.mapped && !workload.debuggerCamera.enabled) {
                const GameFrameMap::ViewProjectionMap &viewProjMap = workloadMap.viewProjections[proj.transformsIndex];
                if (viewProjMap.mapped) {
                    const Workload &prevWorkload = p.workloadQueue->workloads[workloadMap.prevWorkloadIndex];
                    tracePreviousData = &prevWorkload.drawData;
                    tracePreviousWorkload = prevWorkload.workloadId;
                    const uint32_t currentGroup = drawData.viewProjTransformGroups[proj.transformsIndex];
                    const bool gameplayViewportTransition =
                        (drawData.transformGroups[currentGroup].matrixId == 0x00001000U) &&
                        !sameProjectionViewport(prevWorkload.drawData, viewProjMap.prevTransformIndex,
                                                drawData, proj.transformsIndex);
                    traceViewportBypass = gameplayViewportTransition;
                    if (gameplayViewportTransition) {
                        // Only Tooie's explicitly tagged gameplay cameras opt
                        // into this transition rule. Use BOTH submitted camera
                        // and projection; do not change the viewport or aspect.
                        g_bt_rt64_projection_viewport_bypasses.fetch_add(1, std::memory_order_relaxed);
                    }
                    else {
                        prevViewMatrix = &prevWorkload.drawData.viewTransforms[viewProjMap.prevTransformIndex];
                        prevProjMatrix = &prevWorkload.drawData.projTransforms[viewProjMap.prevTransformIndex];
                        rigidBody = &viewProjMap.rigidBody;
                    }
                    traceMapped = true;
                    tracePrevPairedTi = int32_t(viewProjMap.prevTransformIndex);
                    tracePrevPairedId = prevWorkload.drawData.transformGroups[prevWorkload.drawData.viewProjTransformGroups[viewProjMap.prevTransformIndex]].matrixId;
                }
            }

            const uint32_t curProjGroupIndex = workload.drawData.viewProjTransformGroups[proj.transformsIndex];
            const TransformGroup &curProjGroup = workload.drawData.transformGroups[curProjGroupIndex];
            bool adjustAspectRatio = (curProjGroup.aspectMode == G_EX_ASPECT_ADJUST);
            if (curProjGroup.aspectMode == G_EX_ASPECT_AUTO) {
                FixedRect intersectionRect = proj.scissorRect;
                if (proj.usesViewport()) {
                    const interop::RSPViewport &viewport = drawData.rspViewports[proj.transformsIndex];
                    const int16_t *viewportClipRatios = &drawData.viewportClipRatios[proj.transformsIndex * 4];
                    intersectionRect = intersectionRect.intersection(viewport.rect(viewportClipRatios));
                }

                if (!intersectionRect.isEmpty()) {
                    bool coversWholeWidth = (intersectionRect.ulx <= fbPair.scissorRect.ulx) && (intersectionRect.lrx >= fbPair.scissorRect.lrx);
                    bool horizontalRatio = (intersectionRect.width(true, true) > intersectionRect.height(true, true));
                    adjustAspectRatio = (viewportOrigin == G_EX_ORIGIN_NONE) && coversWholeWidth && horizontalRatio;
                }
            }
 
            const bool isSkyboxProjection = (curProjGroup.matrixId == 0x00001001U);
            if (adjustAspectRatio) {
                (isSkyboxProjection ? g_bt_rt64_proj_adjusted_sky : g_bt_rt64_proj_adjusted_other)
                    .fetch_add(1, std::memory_order_relaxed);
            }
            else {
                (isSkyboxProjection ? g_bt_rt64_proj_skipped_sky : g_bt_rt64_proj_skipped_other)
                    .fetch_add(1, std::memory_order_relaxed);
            }

            float projRatioScale = adjustAspectRatio ? (1.0f / p.aspectRatioScale) : 1.0f;
            interop::float4x4 &viewMatrix = drawData.modViewTransforms[proj.transformsIndex];
            interop::float4x4 &projMatrix = drawData.modProjTransforms[proj.transformsIndex];
            interop::float4x4 &viewProjMatrix = drawData.modViewProjTransforms[proj.transformsIndex];
            viewMatrix = drawData.viewTransforms[proj.transformsIndex];
            projMatrix = drawData.projTransforms[proj.transformsIndex];
            viewProjMatrix = drawData.viewProjTransforms[proj.transformsIndex];

            // Debugger camera.
            if (workload.debuggerCamera.enabled && (proj.type == Projection::Type::Perspective) && (workload.debuggerCamera.sceneIndex == sceneIndex)) {
                viewMatrix = workload.debuggerCamera.viewMatrix;
                projMatrix = workload.debuggerCamera.projMatrix;
            }

            adjustProjectionMatrix(projMatrix, projRatioScale);

            // TEMPORARY diagnostic: give the skybox the first-person
            // infinite-far-plane projection so its behaviour can be reproduced
            // from the attract loop.
            if ((curProjGroup.matrixId == 0x00001001U) && skyInfiniteEnabled()) {
                makeInfiniteFarPlane(projMatrix);
            }

            interop::float4x4 &prevViewTransform = drawData.prevViewTransforms[proj.transformsIndex];
            interop::float4x4 &prevProjTransform = drawData.prevProjTransforms[proj.transformsIndex];
            bool traceInterpolateProjection = false; // TEMPORARY diagnostic
            if ((prevProjMatrix != nullptr) && (prevViewMatrix != nullptr) && (rigidBody != nullptr)) {
                const interop::float4x4 curViewTransform = viewMatrix;
                const interop::float4x4 curProjTransform = projMatrix;
                interop::float4x4 adjustedPrevProj = *prevProjMatrix;
                adjustProjectionMatrix(adjustedPrevProj, projRatioScale);
                viewMatrix = rigidBody->lerp(p.curFrameWeight, *prevViewMatrix, curViewTransform, true);
                prevViewTransform = rigidBody->lerp(p.prevFrameWeight, *prevViewMatrix, curViewTransform, true);

                // We only interpolate the projection if the view matrix has been interpolated.
                const bool interpolateProjection = rigidBody->lerpTranslation || rigidBody->lerpRotation;
                traceInterpolateProjection = interpolateProjection;
                if (interpolateProjection) {
                    projMatrix = lerpMatrix(adjustedPrevProj, curProjTransform, p.curFrameWeight);
                    prevProjTransform = lerpMatrix(adjustedPrevProj, curProjTransform, p.prevFrameWeight);
                }
                else {
                    projMatrix = curProjTransform;
                    prevProjTransform = curProjTransform;
                }
            }
            else {
                prevViewTransform = viewMatrix;
                prevProjTransform = projMatrix;
            }

            viewProjMatrix = hlslpp::mul(viewMatrix, projMatrix);

            interop::float4x4 &prevViewProjTransform = drawData.prevViewProjTransforms[proj.transformsIndex];
            prevViewProjTransform = hlslpp::mul(prevViewTransform, prevProjTransform);

            cameraDiagnostics::observe({workload.workloadId, tracePreviousWorkload, sceneIndex, i,
                proj.transformsIndex, curProjGroup.matrixId, tracePrevPairedId, tracePrevPairedTi,
                p.curFrameWeight, p.prevFrameWeight, p.aspectRatioScale, projRatioScale,
                traceMapped, traceViewportBypass, adjustAspectRatio, workload.debuggerCamera.enabled},
                drawData, tracePreviousData, proj, fbPair);

            // TEMPORARY diagnostic (Banjo-Tooie port): see traceProjEnabled above.
            // splitErr0 is the RSP split's own round-trip error before any
            // modification; splitErr1 is the same after the aspect adjustment and
            // the interpolation, i.e. exactly how far the matrix RT64 renders
            // with has moved from the one the game loaded.
            if (traceProjEnabled()) {
                const interop::float4x4 &rawView = drawData.viewTransforms[proj.transformsIndex];
                const interop::float4x4 &rawProj = drawData.projTransforms[proj.transformsIndex];
                const interop::float4x4 &rawViewProj = drawData.viewProjTransforms[proj.transformsIndex];
                const interop::float4x4 &modView = drawData.modViewTransforms[proj.transformsIndex];
                const interop::float4x4 &modProj = drawData.modProjTransforms[proj.transformsIndex];
                const interop::float4x4 &modViewProj = drawData.modViewProjTransforms[proj.transformsIndex];
                std::fprintf(stderr,
                    "[proj] f=%u scene=%zu p=%zu ti=%u id=0x%08X %s adjust=%d scale=%.3f w=%.3f mapped=%d prevTi=%d prevId=0x%08X interp=%d\n",
                    traceProjFrame, sceneIndex, i, proj.transformsIndex, curProjGroup.matrixId,
                    (proj.type == Projection::Type::Perspective) ? "persp" : "ortho",
                    adjustAspectRatio ? 1 : 0, projRatioScale, p.curFrameWeight,
                    traceMapped ? 1 : 0, tracePrevPairedTi, tracePrevPairedId, traceInterpolateProjection ? 1 : 0);
                std::fprintf(stderr,
                    "       splitErr0=%.6f splitErr1=%.6f vpDelta=%.6f rawViewIdent=%d rawProjIdent=%d\n",
                    matrixDifference(hlslpp::mul(rawView, rawProj), rawViewProj),
                    matrixDifference(hlslpp::mul(modView, modProj), modViewProj),
                    matrixDifference(modViewProj, rawViewProj),
                    isMatrixIdentity(rawView) ? 1 : 0, isMatrixIdentity(rawProj) ? 1 : 0);
                // `cam` is the camera position the game's own matrix was built
                // for; `modCam` is the one RT64 will actually render with, and
                // `camDelta` is the distance between them. For the skybox -- a dome
                // anchored to the camera -- a non-zero camDelta moves the dome
                // relative to the frame, which is what reveals the sky draw's own
                // black fill at the edges.
                const float camDelta = std::sqrt(
                    (modView[3][0] - rawView[3][0]) * (modView[3][0] - rawView[3][0]) +
                    (modView[3][1] - rawView[3][1]) * (modView[3][1] - rawView[3][1]) +
                    (modView[3][2] - rawView[3][2]) * (modView[3][2] - rawView[3][2]));
                std::fprintf(stderr,
                    "       rawProj n=%.3f f=%.3f fov=%.3f infRaw=%d | modProj n=%.3f f=%.3f fov=%.3f infMod=%d | modZ=[%.4f %.4f %.4f %.4f] rawZ=[%.4f %.4f %.4f %.4f] | cam=[%.1f %.1f %.1f] modCam=[%.1f %.1f %.1f] camDelta=%.3f\n",
                    nearPlaneFromProj(rawProj), farPlaneFromProj(rawProj), fovFromProj(rawProj),
                    isInfiniteFarPlane(rawProj) ? 1 : 0,
                    nearPlaneFromProj(modProj), farPlaneFromProj(modProj), fovFromProj(modProj),
                    isInfiniteFarPlane(modProj) ? 1 : 0,
                    modProj[2][0], modProj[2][1], modProj[2][2], modProj[2][3],
                    rawProj[2][0], rawProj[2][1], rawProj[2][2], rawProj[2][3],
                    rawView[3][0], rawView[3][1], rawView[3][2],
                    modView[3][0], modView[3][1], modView[3][2], camDelta);
                // Dump the matrices whenever the projection's far plane is not a
                // normal finite one -- the infinite-far form, or an interpolated
                // far plane far outside anything the game itself asked for.
                const bool traceInteresting = isInfiniteFarPlane(rawProj) || isInfiniteFarPlane(modProj) ||
                    (farPlaneFromProj(modProj) > 100000.0f);
                if (traceInteresting && (traceMatrixDumps < 60)) {
                    traceMatrixDumps++;
                    std::fprintf(stderr, "       DEGENERATE f=%u scene=%zu p=%zu ti=%u id=0x%08X type=%s adjust=%d w=%.3f mapped=%d prevTi=%d prevId=0x%08X\n",
                        traceProjFrame, sceneIndex, i, proj.transformsIndex, curProjGroup.matrixId,
                        (proj.type == Projection::Type::Perspective) ? "persp" : "ortho",
                        adjustAspectRatio ? 1 : 0, p.curFrameWeight, traceMapped ? 1 : 0, tracePrevPairedTi, tracePrevPairedId);
                    traceProjPrintMatrix("rawViewProj", rawViewProj);
                    traceProjPrintMatrix("rawView    ", rawView);
                    traceProjPrintMatrix("rawProj    ", rawProj);
                    traceProjPrintMatrix("modView    ", modView);
                    traceProjPrintMatrix("modProj    ", modProj);
                }
            }
        }
    }

    void ProjectionProcessor::upload(const ProcessParams &p) {
        uploads.clear();

        for (uint32_t w : p.curFrame->workloads) {
            Workload &workload = p.workloadQueue->workloads[w];
            const DrawData &drawData = workload.drawData;
            DrawBuffers &drawBuffers = workload.drawBuffers;
            std::pair<size_t, size_t> uploadRange = { 0, drawData.viewProjTransforms.size() };
            uploads.emplace_back(BufferUploader::Upload{ drawData.modViewProjTransforms.data(), uploadRange, sizeof(interop::float4x4), RenderBufferFlag::STORAGE, { }, &drawBuffers.viewProjTransformsBuffer });
        }

        bufferUploader->submit(p.worker, uploads);
    }
};