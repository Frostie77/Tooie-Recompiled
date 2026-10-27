// Opt-in copies of the CPU inputs selected for GPU upload. No matching,
// interpolation, rendering or pacing decisions may depend on this observer.
#pragma once
#include "hle/rt64_game_frame.h"
#include "hle/rt64_workload.h"
#include "../../../../src/runtime_diagnostics.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace RT64::sceneDiagnostics {
constexpr unsigned MaxProjections = 16, MaxCalls = 256, MaxTransforms = 256;
constexpr unsigned MaxVerticesPerTransform = 4, MaxIndices = 131072;
constexpr unsigned MaxCallVisits = 4096;
constexpr uint64_t PeriodUs = 5'000'000;
constexpr const char *RequestName = "render-trace-v1";
constexpr const char *RequestContents = "tooie-render-trace-v1\n";

// The private request file is explicitly staged for a diagnostic test. Absent
// or malformed requests are OFF. Polling also lets removal stop an active trace
// without restarting the app. No game settings or saves are changed here.
struct Window {
    uint64_t checked = 0, sampled = 0;
    bool initialized = false, captured = false, enabled = false;
    bool due(uint64_t now) {
        if (!initialized || now < checked || now - checked >= PeriodUs) {
            initialized = true; checked = now; enabled = false;
#if defined(__ANDROID__)
            const char *folder = std::getenv("APP_FOLDER_PATH");
            char path[1024];
            const int n = folder ? std::snprintf(path, sizeof(path), "%s/%s", folder, RequestName) : -1;
            if (n > 0 && size_t(n) < sizeof(path)) {
                if (FILE *stream = std::fopen(path, "rb")) {
                    char contents[32]; const size_t length = std::fread(contents, 1, sizeof(contents), stream);
                    enabled = !std::ferror(stream) && length == std::strlen(RequestContents) &&
                        std::memcmp(contents, RequestContents, length) == 0;
                    std::fclose(stream);
                }
            }
#endif
        }
        if (!enabled || (captured && now >= sampled && now - sampled < PeriodUs)) return false;
        captured = true; sampled = now; return true;
    }
};

struct Context {
    uint64_t sample = 0;
    float weight = 1, previousWeight = 0;
    bool transformsProcessed = false, projectionsProcessed = false;
    float sourceAspect = 0, targetAspect = 0, extendedPercentage = 0;
};
struct Counts { unsigned projections = 0, calls = 0, transforms = 0, vertices = 0, inspected = 0, invalid = 0; bool truncated = false; };
inline void matrix(const Context &c, uint64_t workload, const char *kind, unsigned index,
                   const char *endpoint, const hlslpp::float4x4 &input) {
    alignas(16) const hlslpp::float4x4 m = input;
    char line[1024];
    std::snprintf(line, sizeof(line), "sample=%llu workload=%llu kind=%s index=%u matrix=%s rows=%.9g/%.9g/%.9g/%.9g;%.9g/%.9g/%.9g/%.9g;%.9g/%.9g/%.9g/%.9g;%.9g/%.9g/%.9g/%.9g",
        (unsigned long long)c.sample, (unsigned long long)workload, kind, index, endpoint,
        double(m[0][0]), double(m[0][1]), double(m[0][2]), double(m[0][3]), double(m[1][0]), double(m[1][1]), double(m[1][2]), double(m[1][3]),
        double(m[2][0]), double(m[2][1]), double(m[2][2]), double(m[2][3]), double(m[3][0]), double(m[3][1]), double(m[3][2]), double(m[3][3]));
    tooie::diagnostics::log("scene-matrix", line);
}
inline hlslpp::float4x4 consumed(const DrawData &d, unsigned t, bool processed) {
    return processed && t < d.lerpWorldTransforms.size() ? d.lerpWorldTransforms[t] : d.worldTransforms[t];
}
inline Counts report(const Workload &w, const Workload *previous,
                     const GameFrameMap::WorkloadMap *map, const Context &c) {
    Counts out;
    if (!std::isfinite(c.weight) || !std::isfinite(c.previousWeight) || c.weight < 0 || c.weight > 1 ||
        c.previousWeight < 0 || c.previousWeight > 1) { out.invalid = 1; return out; }
    const auto start = tooie::diagnostics::monotonicUs();
    const auto &d = w.drawData;
    const bool history = c.transformsProcessed && previous && map && map->mapped;
    char line[1500];
    std::snprintf(line, sizeof(line), "sample=%llu workload=%llu previous_workload=%llu weight=%.9g previous_weight=%.9g transforms_processed=%u projections_processed=%u source_aspect=%.9g target_aspect=%.9g extended_percentage=%.9g world_matrices=%zu source_vertices=%u observation=cpu-inputs-not-gpu-readback",
        (unsigned long long)c.sample, (unsigned long long)w.workloadId, (unsigned long long)(previous ? previous->workloadId : 0),
        double(c.weight), double(c.previousWeight), unsigned(c.transformsProcessed), unsigned(c.projectionsProcessed),
        double(c.sourceAspect), double(c.targetAspect), double(c.extendedPercentage), d.worldTransforms.size(), d.vertexCount());
    tooie::diagnostics::log("scene-sample", line);

    // Report raw and selected projection inputs, including endpoint-only runs.
    // A stale mod* buffer is never labelled consumed when its processor is OFF.
    for (unsigned i = 0; i < d.viewProjTransforms.size() && i < MaxProjections; ++i) {
        ++out.projections;
        const bool mapped = history && i < map->viewProjections.size() && map->viewProjections[i].mapped;
        const unsigned old = mapped ? map->viewProjections[i].prevTransformIndex : UINT32_MAX;
        const unsigned group = i < d.viewProjTransformGroups.size() ? d.viewProjTransformGroups[i] : UINT32_MAX;
        const unsigned id = group < d.transformGroups.size() ? d.transformGroups[group].matrixId : G_EX_ID_AUTO;
        const bool viewport = i < d.rspViewports.size();
        std::snprintf(line, sizeof(line), "sample=%llu workload=%llu projection=%u id=0x%X mapped=%u previous_projection=%u viewport_origin=%u has_viewport=%u viewport_scale=%.9g/%.9g/%.9g viewport_translate=%.9g/%.9g/%.9g",
            (unsigned long long)c.sample, (unsigned long long)w.workloadId, i, id, unsigned(mapped), old,
            i < d.viewportOrigins.size() ? unsigned(d.viewportOrigins[i]) : UINT32_MAX, unsigned(viewport),
            viewport ? double(d.rspViewports[i].scale[0]) : 0, viewport ? double(d.rspViewports[i].scale[1]) : 0, viewport ? double(d.rspViewports[i].scale[2]) : 0,
            viewport ? double(d.rspViewports[i].translate[0]) : 0, viewport ? double(d.rspViewports[i].translate[1]) : 0, viewport ? double(d.rspViewports[i].translate[2]) : 0);
        tooie::diagnostics::log("scene-projection", line);
        matrix(c, w.workloadId, "projection", i, "current", d.viewProjTransforms[i]);
        matrix(c, w.workloadId, "projection", i, "consumed", c.projectionsProcessed && i < d.modViewProjTransforms.size() ? d.modViewProjTransforms[i] : d.viewProjTransforms[i]);
        if (i < d.viewTransforms.size()) matrix(c, w.workloadId, "view", i, "current", d.viewTransforms[i]);
        if (i < d.projTransforms.size()) matrix(c, w.workloadId, "projection-only", i, "current", d.projTransforms[i]);
        if (c.projectionsProcessed && i < d.modViewTransforms.size()) matrix(c, w.workloadId, "view", i, "consumed", d.modViewTransforms[i]);
        if (c.projectionsProcessed && i < d.modProjTransforms.size()) matrix(c, w.workloadId, "projection-only", i, "consumed", d.modProjTransforms[i]);
        if (mapped && old < previous->drawData.viewProjTransforms.size())
            matrix(c, w.workloadId, "projection", i, "previous", previous->drawData.viewProjTransforms[old]);
    }
    out.truncated |= d.viewProjTransforms.size() > MaxProjections;

    struct Pick { unsigned vertex, framebuffer, projection, call, projectionTransform; };
    std::unordered_map<unsigned, std::vector<Pick>> picks;
    std::vector<unsigned> order;
    // Prioritize actual drawn owned character bones before terrain/unowned
    // geometry. Both passes are bounded; all omissions are explicitly reported.
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned inspected = 0, visits = 0;
        for (unsigned f = 0; f < w.fbPairCount && f < w.fbPairs.size(); ++f) {
            if (inspected >= MaxIndices || visits >= MaxCallVisits) { out.truncated = true; break; }
            const auto &fb = w.fbPairs[f];
            for (unsigned p = 0; p < fb.projectionCount && p < fb.projections.size(); ++p) {
                if (inspected >= MaxIndices || visits >= MaxCallVisits) { out.truncated = true; break; }
                const auto &proj = fb.projections[p];
                for (unsigned a = 0; a < proj.gameCallCount && a < proj.gameCalls.size(); ++a) {
                    if (inspected >= MaxIndices || visits++ >= MaxCallVisits) { out.truncated = true; break; }
                    const auto &call = proj.gameCalls[a];
                    const bool indexed = proj.type == Projection::Type::Perspective || proj.type == Projection::Type::Orthographic;
                    const unsigned projectionTransform = indexed ? proj.transformsIndex : UINT32_MAX;
                    if (pass == 0 && out.calls < MaxCalls) {
                        ++out.calls;
                        std::snprintf(line, sizeof(line), "sample=%llu workload=%llu fb=%u projection=%u projection_transform=%u call=%u uid=%u type=%u triangles=%u rect=%d/%d/%d/%d rect_origins=%u/%u scissor=%d/%d/%d/%d scissor_origins=%u/%u",
                            (unsigned long long)c.sample, (unsigned long long)w.workloadId, f, p, projectionTransform, a, call.callDesc.uid, unsigned(proj.type), call.callDesc.triangleCount,
                            call.callDesc.rect.ulx, call.callDesc.rect.uly, call.callDesc.rect.lrx, call.callDesc.rect.lry,
                            unsigned(call.callDesc.rectLeftOrigin), unsigned(call.callDesc.rectRightOrigin),
                            call.callDesc.scissorRect.ulx, call.callDesc.scissorRect.uly, call.callDesc.scissorRect.lrx, call.callDesc.scissorRect.lry,
                            unsigned(call.callDesc.scissorLeftOrigin), unsigned(call.callDesc.scissorRightOrigin));
                        tooie::diagnostics::log("scene-call", line);
                    } else if (pass == 0) out.truncated = true;
                    if (!indexed) continue;
                    const uint64_t first = call.meshDesc.faceIndicesStart, count = uint64_t(call.callDesc.triangleCount) * 3;
                    if (first > d.faceIndices.size() || count > d.faceIndices.size() - first) { ++out.invalid; continue; }
                    for (uint64_t j = first; j < first + count; ++j) {
                        if (inspected++ >= MaxIndices) { out.truncated = true; break; }
                        ++out.inspected;
                        const unsigned v = d.faceIndices[j];
                        if (v >= d.worldIndices.size() || v >= d.posFloats.size()/3 || v >= d.velFloats.size()/3) { ++out.invalid; continue; }
                        const unsigned t = d.worldIndices[v];
                        if (t >= d.worldTransforms.size()) { ++out.invalid; continue; }
                        bool owned = false;
#if TOOIE_SKELETAL_POSE_ENABLED
                        owned = d.skeletalTransforms.count(t) != 0;
#endif
                        if (owned != (pass == 0)) continue;
                        auto found = picks.find(t);
                        if (found == picks.end()) {
                            if (picks.size() >= MaxTransforms) { out.truncated = true; continue; }
                            order.push_back(t); found = picks.emplace(t, std::vector<Pick>{}).first;
                        }
                        auto &vertices = found->second;
                        if (vertices.size() < MaxVerticesPerTransform && std::none_of(vertices.begin(), vertices.end(), [v](const Pick &x) { return x.vertex == v; }))
                            vertices.push_back({v, f, p, a, projectionTransform});
                    }
                }
            }
        }
    }
    for (unsigned t : order) {
        ++out.transforms;
        const bool mapped = history && t < map->transforms.size() && map->transforms[t].mapped;
        const unsigned old = mapped ? map->transforms[t].prevTransformIndex : UINT32_MAX;
        const unsigned group = t < d.worldTransformGroups.size() ? d.worldTransformGroups[t] : UINT32_MAX;
        const auto *g = group < d.transformGroups.size() ? &d.transformGroups[group] : nullptr;
        const auto *before = mapped && old < previous->drawData.worldTransforms.size() ? &previous->drawData.worldTransforms[old] : nullptr;
        std::snprintf(line, sizeof(line), "sample=%llu workload=%llu transform=%u id=0x%X mapped=%u previous_transform=%u vertex_mode=%u position_mode=%u rotation_mode=%u decompose=%u matrix_address=0x%X previous_matrix_address=0x%X addresses_valid=%u/%u",
            (unsigned long long)c.sample, (unsigned long long)w.workloadId, t, g ? g->matrixId : G_EX_ID_AUTO, unsigned(mapped), old,
            g ? unsigned(g->vertexInterpolation) : UINT32_MAX, g ? unsigned(g->positionInterpolation) : UINT32_MAX,
            g ? unsigned(g->rotationInterpolation) : UINT32_MAX, g ? unsigned(g->decompose) : 0,
            t < d.worldTransformPhysicalAddresses.size() ? d.worldTransformPhysicalAddresses[t] : 0,
            before && old < previous->drawData.worldTransformPhysicalAddresses.size() ? previous->drawData.worldTransformPhysicalAddresses[old] : 0,
            unsigned(t < d.worldTransformPhysicalAddresses.size()), unsigned(before && old < previous->drawData.worldTransformPhysicalAddresses.size()));
        tooie::diagnostics::log("scene-transform", line);
        matrix(c, w.workloadId, "world", t, "current", d.worldTransforms[t]);
        matrix(c, w.workloadId, "world", t, "consumed", consumed(d, t, c.transformsProcessed));
        if (before) matrix(c, w.workloadId, "world", t, "previous", *before);
        if (c.transformsProcessed && t < d.prevWorldTransforms.size()) matrix(c, w.workloadId, "world", t, "consumed_previous", d.prevWorldTransforms[t]);
#if TOOIE_SKELETAL_POSE_ENABLED
        const auto binding = d.skeletalTransforms.find(t);
        if (binding != d.skeletalTransforms.end() && binding->second && binding->second->pose) {
            const auto &b = *binding->second; const auto &pose = *b.pose;
            const auto pair = mapped ? map->transforms[t].skeletalPair : nullptr;
            std::snprintf(line, sizeof(line), "sample=%llu workload=%llu transform=%u player=%u actor=0x%X generation=%llu model=0x%X source_frame=%llu bone=%u bones=%zu paired=%u previous_source_frame=%llu",
                (unsigned long long)c.sample, (unsigned long long)w.workloadId, t, pose.player, pose.actor,
                (unsigned long long)pose.actorGeneration, pose.model, (unsigned long long)pose.frame, b.bone, pose.bones.size(), unsigned(bool(pair)),
                (unsigned long long)(pair ? pair->previous->frame : 0));
            tooie::diagnostics::log("scene-owner", line);
            matrix(c, w.workloadId, "source-root", t, "current", skeletal::matrix(pose.root));
            if (b.bone < pose.bones.size()) matrix(c, w.workloadId, "source-bone", t, "current", skeletal::matrix(pose.bones[b.bone].matrix));
            if (pair && b.bone < pair->previous->bones.size()) {
                matrix(c, w.workloadId, "source-root", t, "previous", skeletal::matrix(pair->previous->root));
                matrix(c, w.workloadId, "source-bone", t, "previous", skeletal::matrix(pair->previous->bones[b.bone].matrix));
            }
        }
#endif
        for (const auto &pick : picks.at(t)) {
            ++out.vertices; const unsigned v = pick.vertex;
            const hlslpp::float3 pos(d.posFloats[v*3], d.posFloats[v*3+1], d.posFloats[v*3+2]);
            const hlslpp::float3 vel(d.velFloats[v*3], d.velFloats[v*3+1], d.velFloats[v*3+2]);
            const auto world = hlslpp::mul(hlslpp::float4(pos - vel * (1-c.weight), 1), consumed(d, t, c.transformsProcessed));
            // Copy a previous source position independently of velocity. A
            // SKIP/mismatched velocity must not masquerade as unchanged source
            // geometry. The range-offset candidate is labelled, not asserted
            // to be a topologically proven vertex correspondence.
            unsigned previousVertex = UINT32_MAX;
            hlslpp::float3 previousSource(0);
            if (before && t < d.worldTransformVertexIndices.size() && old < previous->drawData.worldTransformVertexIndices.size()) {
                const auto &p = previous->drawData;
                const uint64_t first = d.worldTransformVertexIndices[t];
                const uint64_t end = t+1 < d.worldTransformVertexIndices.size() ? d.worldTransformVertexIndices[t+1] : d.vertexCount();
                const uint64_t pfirst = p.worldTransformVertexIndices[old];
                const uint64_t pend = old+1 < p.worldTransformVertexIndices.size() ? p.worldTransformVertexIndices[old+1] : p.vertexCount();
                if (first <= v && v < end && end <= d.vertexCount() && pfirst <= pend && pend <= p.vertexCount() && end-first == pend-pfirst) {
                    const uint64_t candidate = pfirst+v-first;
                    if (candidate < pend && candidate < p.posFloats.size()/3 && p.worldIndices[candidate] == old) {
                        previousVertex = unsigned(candidate);
                        previousSource = hlslpp::float3(p.posFloats[candidate*3], p.posFloats[candidate*3+1], p.posFloats[candidate*3+2]);
                    }
                }
            }
            std::snprintf(line, sizeof(line), "sample=%llu workload=%llu transform=%u vertex=%u fb=%u projection=%u projection_transform=%u call=%u pos=%.9g/%.9g/%.9g vel=%.9g/%.9g/%.9g consumed_world=%.9g/%.9g/%.9g/%.9g prev_source_valid=%u prev_vertex=%u prev_source=%.9g/%.9g/%.9g prev_source_basis=matched-range-offset-not-proven-topology",
                (unsigned long long)c.sample, (unsigned long long)w.workloadId, t, v, pick.framebuffer, pick.projection, pick.projectionTransform, pick.call,
                double(pos.x), double(pos.y), double(pos.z), double(vel.x), double(vel.y), double(vel.z), double(world.x), double(world.y), double(world.z), double(world.w),
                unsigned(previousVertex != UINT32_MAX), previousVertex, double(previousSource.x), double(previousSource.y), double(previousSource.z));
            tooie::diagnostics::log("scene-vertex", line);
        }
    }
    std::snprintf(line, sizeof(line), "sample=%llu workload=%llu projections=%u calls=%u transforms=%u vertices=%u inspected_indices=%u invalid=%u truncated=%u max_projections=%u max_calls=%u max_transforms=%u vertices_per_transform=%u max_indices_per_pass=%u observer_us=%llu includes_log_io=1 selection=drawn-owned-first-then-unowned-no-visual-identity-claim",
        (unsigned long long)c.sample, (unsigned long long)w.workloadId, out.projections, out.calls, out.transforms, out.vertices, out.inspected, out.invalid, unsigned(out.truncated),
        MaxProjections, MaxCalls, MaxTransforms, MaxVerticesPerTransform, MaxIndices, (unsigned long long)(tooie::diagnostics::monotonicUs()-start));
    tooie::diagnostics::log("scene-budget", line);
    return out;
}
} // namespace RT64::sceneDiagnostics
