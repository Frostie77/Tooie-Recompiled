//
// RT64
//

#include "rt64_transform_processor.h"

#include "common/rt64_math.h"
#include "hle/rt64_game_frame.h"
#include "hle/rt64_workload_queue.h"
#include "rt64_correction_diagnostics.h"

#if defined(__ANDROID__) && defined(TOOIE_ANDROID_GEOMETRY_TRACE) && TOOIE_ANDROID_GEOMETRY_TRACE
#include "rt64_geometry_diagnostics.h"
#endif

namespace RT64 {
    // TransformProcessor
    
    TransformProcessor::TransformProcessor() { }

    TransformProcessor::~TransformProcessor() { }

    void TransformProcessor::setup(RenderWorker *worker) {
        bufferUploader = std::make_unique<BufferUploader>(worker->device);
    }

    void TransformProcessor::process(const ProcessParams &p) {
#if defined(__ANDROID__) && defined(TOOIE_ANDROID_GEOMETRY_TRACE) && TOOIE_ANDROID_GEOMETRY_TRACE
        static thread_local geometryDiagnostics::Window geometryWindow;
        const uint64_t geometrySample = tooie::diagnostics::monotonicUs();
        const bool captureGeometry = geometryWindow.due(geometrySample, p.curFrameWeight);
#endif
        for (uint32_t w : p.curFrame->workloads) {
            Workload &workload = p.workloadQueue->workloads[w];
            DrawData &drawData = workload.drawData;
            const bool prevFrameValid = (p.prevFrame != nullptr) && p.curFrame->frameMap.workloads[w].mapped;
            auto &lerpWorldTransforms = drawData.lerpWorldTransforms;
            auto &invTWorldTransforms = drawData.invTWorldTransforms;
            auto &prevWorldTransforms = drawData.prevWorldTransforms;
            lerpWorldTransforms.clear();
            invTWorldTransforms.clear();
            prevWorldTransforms.clear();

            hlslpp::float4x4 prevMatrix, curMatrix, invMatrix, invTMatrix;

            // Match with the previous frame and interpolate the transforms.
            if (prevFrameValid) {
                const GameFrameMap::WorkloadMap &workloadMap = p.curFrame->frameMap.workloads[w];
                const DrawData &prevDrawData = p.workloadQueue->workloads[workloadMap.prevWorkloadIndex].drawData;
#if TOOIE_SKELETAL_POSE_ENABLED
                struct PoseResult { std::shared_ptr<const skeletal::Pair> pair; std::vector<skeletal::M> previous, current; bool valid; };
                std::vector<PoseResult> poses;
                unsigned skeletalApplied = 0, skeletalFrozen = 0;
#endif
                for (size_t t = 0; t < drawData.worldTransforms.size(); t++) {
                    const GameFrameMap::TransformMap &transformMap = workloadMap.transforms[t];
                    if (transformMap.mapped) {
                        const hlslpp::float4x4 &prevTransform = prevDrawData.worldTransforms[workloadMap.transforms[t].prevTransformIndex];
                        const hlslpp::float4x4 &curTransform = drawData.worldTransforms[t];
                        prevMatrix = transformMap.rigidBody.lerp(p.prevFrameWeight, prevTransform, curTransform, true);
                        curMatrix = transformMap.rigidBody.lerp(p.curFrameWeight, prevTransform, curTransform, true);
#if TOOIE_SKELETAL_POSE_ENABLED
                        const auto owned = drawData.skeletalTransforms.find(uint32_t(t));
                        if (owned != drawData.skeletalTransforms.end()) {
                            const auto &pair = transformMap.skeletalPair;
                            PoseResult *result = nullptr;
                            if (pair && pair->current == owned->second->pose) {
                                for (auto &pose : poses) if (pose.pair == pair) { result = &pose; break; }
                                if (!result) {
                                    PoseResult pose; pose.pair = pair;
                                    pose.valid = pair->interpolate(p.prevFrameWeight, pose.previous) && pair->interpolate(p.curFrameWeight, pose.current);
                                    poses.push_back(std::move(pose)); result = &poses.back();
                                }
                            }
                            if (result && result->valid && owned->second->bone < result->current.size()) {
                                const unsigned bone = owned->second->bone;
                                // The submitted fixed-converted endpoints remain exact.
                                prevMatrix = p.prevFrameWeight == 0 ? prevTransform : p.prevFrameWeight == 1 ? curTransform : result->previous[bone];
                                curMatrix = p.curFrameWeight == 0 ? prevTransform : p.curFrameWeight == 1 ? curTransform : result->current[bone];
                                ++skeletalApplied;
                            }
                            else {
                                // Changed schema, missing history or invalid relative
                                // poses use the current pose, never independently
                                // blended final-world bones of a known skeleton.
                                prevMatrix = curMatrix = curTransform; ++skeletalFrozen;
                            }
                        }
#endif
                        invMatrix = hlslpp::inverse(curMatrix);
                        invTMatrix = hlslpp::transpose(invMatrix);
                        lerpWorldTransforms.emplace_back(curMatrix);
                        invTWorldTransforms.emplace_back(invTMatrix);
                        prevWorldTransforms.emplace_back(prevMatrix);
                    }
                    else {
                        invMatrix = hlslpp::inverse(drawData.worldTransforms[t]);
                        invTMatrix = hlslpp::transpose(invMatrix);
                        lerpWorldTransforms.emplace_back(drawData.worldTransforms[t]);
                        invTWorldTransforms.emplace_back(invTMatrix);
                        prevWorldTransforms.emplace_back(drawData.worldTransforms[t]);
                    }
                }
#if TOOIE_SKELETAL_POSE_ENABLED
                struct Counts { uint64_t next = 0, applied = 0, frozen = 0, pairs = 0; };
                static thread_local Counts counts;
                counts.applied += skeletalApplied; counts.frozen += skeletalFrozen; counts.pairs += poses.size();
                const auto now = tooie::diagnostics::monotonicUs();
                if (now >= counts.next && (counts.applied || counts.frozen)) {
                    char text[300]; std::snprintf(text, sizeof(text), "applied=%llu current_pose_fallback=%llu evaluated_pairs=%llu policy=authored-player-hierarchy npc_policy=unchanged",
                        (unsigned long long)counts.applied, (unsigned long long)counts.frozen, (unsigned long long)counts.pairs);
                    tooie::diagnostics::log("skeletal", text);
                    counts = {}; counts.next = now + 5'000'000;
                }
#endif
            }
            // Copy as normal and just generate the inverse of the transforms.
            else {
                for (size_t t = 0; t < drawData.worldTransforms.size(); t++) {
                    invMatrix = hlslpp::inverse(drawData.worldTransforms[t]);
                    invTMatrix = hlslpp::transpose(invMatrix);
                    invTWorldTransforms.push_back(invTMatrix);
                }
            }
#if defined(__ANDROID__) && defined(TOOIE_ANDROID_GEOMETRY_TRACE) && TOOIE_ANDROID_GEOMETRY_TRACE
            // Copy the actual consumer inputs while this workload and its
            // previous workload are owned here. No pointers escape the call.
            if (captureGeometry && prevFrameValid) {
                const auto start = tooie::diagnostics::monotonicUs();
                const auto &map = p.curFrame->frameMap.workloads[w];
                const auto &previous = p.workloadQueue->workloads[map.prevWorkloadIndex];
                const auto snapshot = geometryDiagnostics::collect(workload, &previous, &map, p.curFrameWeight);
                geometryDiagnostics::report(snapshot, geometrySample, workload.workloadId, p.curFrameWeight,
                    tooie::diagnostics::monotonicUs() - start, p.prevFrameWeight);
            }
#endif
#if TOOIE_CORRECTION_TRACE_ENABLED
            if (prevFrameValid) {
                const auto &map = p.curFrame->frameMap.workloads[w];
                correctionDiagnostics::report(workload, p.workloadQueue->workloads[map.prevWorkloadIndex], map, p.curFrameWeight, p.prevFrameWeight);
            }
#endif
        }
    }
    
    void TransformProcessor::upload(const ProcessParams &p) {
        uploads.clear();

        for (uint32_t w : p.curFrame->workloads) {
            const bool prevFrameValid = (p.prevFrame != nullptr);
            Workload &workload = p.workloadQueue->workloads[w];
            const DrawData &drawData = workload.drawData;
            DrawBuffers &drawBuffers = workload.drawBuffers;
            const interop::float4x4 *worldMatrices = prevFrameValid ? drawData.lerpWorldTransforms.data() : drawData.worldTransforms.data();
            const interop::float4x4 *prevWorldMatrices = prevFrameValid ? drawData.prevWorldTransforms.data() : drawData.worldTransforms.data();
            const interop::float4x4 *invTWorldMatrices = drawData.invTWorldTransforms.data();
            std::pair<size_t, size_t> uploadRange = { 0, drawData.worldTransforms.size() };
            uploads.emplace_back(BufferUploader::Upload{ worldMatrices, uploadRange, sizeof(interop::float4x4), RenderBufferFlag::STORAGE, { }, &drawBuffers.worldTransformsBuffer });
            uploads.emplace_back(BufferUploader::Upload{ prevWorldMatrices, uploadRange, sizeof(interop::float4x4), RenderBufferFlag::STORAGE, { }, &drawBuffers.prevWorldTransformsBuffer });
            uploads.emplace_back(BufferUploader::Upload{ invTWorldMatrices, uploadRange, sizeof(interop::float4x4), RenderBufferFlag::STORAGE, { }, &drawBuffers.invTWorldTransformsBuffer });
        }

        bufferUploader->submit(p.worker, uploads);
    }
};