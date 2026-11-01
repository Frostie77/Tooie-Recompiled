#pragma once
#include "../../../../src/skeletal_pose.h"
#if TOOIE_SKELETAL_POSE_ENABLED
#include "rt64_rigid_body.h"
#include "../../include/rt64_extended_gbi.h"
#include "../../../../src/runtime_diagnostics.h"
#include <cmath>
#include <cstring>

namespace RT64::skeletal {
using M = hlslpp::float4x4;
inline M matrix(const tooie::skeletal::Words &words) {
    alignas(16) M result;
    for (unsigned i = 0; i < 16; ++i) { float f; std::memcpy(&f, &words[i], 4); result[i / 4][i % 4] = f; }
    return result;
}
inline hlslpp::float3 pivot(const tooie::skeletal::Bone &bone) {
    float f[3]; std::memcpy(f, bone.bind.data(), sizeof(f)); return {f[0], f[1], f[2]};
}
inline M translation(hlslpp::float3 xyz) { alignas(16) M m = M::identity(); m[3] = hlslpp::float4(xyz, 1); return m; }
inline M inverseAffine(const M &input) {
    alignas(16) const M value = input;
    alignas(16) const hlslpp::float3x3 inverse = hlslpp::inverse(hlslpp::float3x3(value));
    alignas(16) M result = M::identity();
    for (unsigned i = 0; i < 3; ++i) for (unsigned j = 0; j < 3; ++j) result[i][j] = inverse[i][j];
    result[3] = hlslpp::float4(hlslpp::mul(-value[3].xyz, inverse), 1);
    return result;
}
inline bool affine(const M &input) {
    // GCC's scalar hlslpp matrix type has weaker alignment than its float4
    // indexing API. Copy embedded matrices before using that API.
    alignas(16) const M m = input;
    for (unsigned i = 0; i < 4; ++i) for (unsigned j = 0; j < 4; ++j) if (!std::isfinite(float(m[i][j]))) return false;
    if (m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1) return false;
    const float det = m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1]) - m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0]) + m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);
    return std::isfinite(det) && std::fabs(det) > 1e-7f;
}
inline RigidBody body(const M &a, const M &b) {
    RigidBody value;
    value.updateLinear(a, b, G_EX_COMPONENT_INTERPOLATE);
    value.updateAngular(a, b, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE);
    value.updatePerspective(a, b, G_EX_COMPONENT_INTERPOLATE);
    value.updateDecomposition(a, b, true); return value;
}
struct Pair {
    std::shared_ptr<const tooie::skeletal::Pose> previous, current;
    struct Local { alignas(16) M endpoints[2]; RigidBody body; int parent; hlslpp::float3 bind; };
    std::vector<Local> locals;
    alignas(16) M roots[2]; RigidBody rootBody;

    static std::shared_ptr<const Pair> create(std::shared_ptr<const tooie::skeletal::Pose> a,
                                             std::shared_ptr<const tooie::skeletal::Pose> b, const char **reason = nullptr) {
        auto reject = [reason](const char *message) -> std::shared_ptr<const Pair> { if (reason) *reason = message; return {}; };
        if (!a || !b || a->player > 8 || (a->player && a->actorGeneration) || (!a->player && (!TOOIE_ACTOR_POSE_ENABLED || !a->actorGeneration)) || !a->actor || a->player != b->player || a->actor != b->actor ||
            a->actorGeneration != b->actorGeneration || a->model != b->model ||
            a->modelHeader != b->modelHeader || a->frame == UINT64_MAX || b->frame != a->frame + 1 ||
            a->bones.empty() || a->bones.size() != b->bones.size() || a->bones.size() > 128) return reject("ownership-or-history");
        auto p = std::make_shared<Pair>(); p->previous = std::move(a); p->current = std::move(b);
        p->roots[0] = matrix(p->previous->root); p->roots[1] = matrix(p->current->root);
        if (!affine(p->roots[0]) || !affine(p->roots[1])) return reject("root-matrix");
        p->rootBody = body(p->roots[0], p->roots[1]);
        if (!p->rootBody.transforms[0].valid || !p->rootBody.transforms[1].valid) return reject("root-decomposition");
        for (unsigned i = 0; i < p->previous->bones.size(); ++i) {
            const auto &old = p->previous->bones[i], &now = p->current->bones[i];
            const int parent = int16_t(now.bind[3] & 0xFFFF);
            if (old.bind != now.bind || parent < -1 || parent >= int(i)) return reject("schema-or-parent");
            const auto bind = pivot(now);
            for (unsigned j = 0; j < 3; ++j) if (!std::isfinite(float(bind[j]))) return reject("bind-nonfinite");
            Local local; local.parent = parent; local.bind = bind;
            for (unsigned e = 0; e < 2; ++e) {
                const auto &bones = e ? p->current->bones : p->previous->bones;
                const M value = matrix(bones[i].matrix), par = parent < 0 ? M::identity() : matrix(bones[parent].matrix);
                if (!affine(value) || !affine(par)) return reject("bone-matrix");
                // AEBA8's authored pivot convention, including real effective
                // animated translations and completed modifier effects.
                // An affine inverse keeps the homogeneous row/column exact;
                // general 4x4 inversion introduces a spurious perspective term.
                local.endpoints[e] = hlslpp::mul(hlslpp::mul(hlslpp::mul(translation(bind), value), inverseAffine(par)), translation(-bind));
                if (!affine(local.endpoints[e])) return reject("relative-matrix");
            }
            local.body = body(local.endpoints[0], local.endpoints[1]);
            if (!local.body.transforms[0].valid || !local.body.transforms[1].valid) return reject("relative-decomposition");
            p->locals.push_back(std::move(local));
        }
        return p;
    }
    bool interpolate(float weight, std::vector<M> &world) const {
        if (!std::isfinite(weight) || weight < 0 || weight > 1) return false;
        const M root = weight == 0 ? roots[0] : weight == 1 ? roots[1] : rootBody.lerp(weight, roots[0], roots[1], true);
        if (!affine(root)) return false;
        world.clear(); world.reserve(locals.size());
        for (unsigned i = 0; i < locals.size(); ++i) {
            M model;
            if (weight == 0 || weight == 1) model = matrix((weight == 0 ? previous : current)->bones[i].matrix);
            else {
                // A shared mesh edge can differ from the authored bone pivot.
                // Slerping relative rotation while linearly blending its
                // compensating translation can open that edge between two
                // otherwise welded source poses. Blend the completed model
                // matrices together, then apply one common interpolated root.
                // Endpoint-coincident vertices then remain coincident by
                // linearity. Local hierarchy validation above stays intact.
                const M a = matrix(previous->bones[i].matrix);
                const M b = matrix(current->bones[i].matrix);
                for (unsigned row = 0; row < 4; ++row)
                    model[row] = a[row] * (1.0f - weight) + b[row] * weight;
            }
            if (!affine(model)) return false;
            world.push_back(model);
        }
        for (auto &value : world) { value = hlslpp::mul(value, root); if (!affine(value)) return false; }
        return true;
    }
};
}
#endif
