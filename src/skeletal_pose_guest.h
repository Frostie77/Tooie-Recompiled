#pragma once
#include "skeletal_pose.h"
#if TOOIE_SKELETAL_POSE_ENABLED
#include <cmath>
#include <cstring>
#include "runtime_diagnostics.h"

namespace tooie::skeletal::guest {
inline bool valid(uint32_t p, size_t bytes) { return !(p & 3) && p >= 0x80000000U && p <= 0x80800000U && bytes <= 0x80800000U - p; }
inline uint32_t word(const uint8_t *ram, uint32_t p) {
    uint32_t value = 0; if (valid(p, 4)) std::memcpy(&value, ram + (p & 0x7FFFFF), 4); return value;
}
template<size_t N> bool copy(const uint8_t *ram, uint32_t p, std::array<uint32_t, N> &out) {
    if (!valid(p, N * 4)) return false;
    std::memcpy(out.data(), ram + (p & 0x7FFFFF), N * 4); return true;
}
struct Builder { uint32_t output = 0, skeleton = 0; std::vector<Bone> bones; };
struct Source {
    std::unordered_map<uint32_t, std::shared_ptr<const Builder>> builders;
    std::vector<std::unique_ptr<Builder>> pending;
    uint64_t frame = 0;
    uint64_t nextReport = 0, buildersSeen = 0, published = 0;
    std::array<uint64_t, 5> rejected{};
};
inline Source &source() { static thread_local Source value; return value; }
inline std::shared_ptr<const Pose> reject(unsigned reason) { ++source().rejected[reason]; return {}; }
inline void frameBegin() {
    auto &s = source(); ++s.frame;
    const uint64_t now = diagnostics::monotonicUs();
    if (now < s.nextReport) return;
    uint64_t rejections = 0; for (auto n : s.rejected) rejections += n;
    if (s.buildersSeen || s.published || rejections) {
        char text[400]; std::snprintf(text, sizeof(text),
            "frame=%llu builders=%llu published=%llu rejected_owner=%llu missing_builder=%llu schema_mismatch=%llu changed_builder_output=%llu invalid_copy=%llu npc_policy=unchanged",
            (unsigned long long)s.frame, (unsigned long long)s.buildersSeen, (unsigned long long)s.published,
            (unsigned long long)s.rejected[0], (unsigned long long)s.rejected[1], (unsigned long long)s.rejected[2],
            (unsigned long long)s.rejected[3], (unsigned long long)s.rejected[4]);
        diagnostics::log("skeletal-source", text);
    }
    s.nextReport = now + 5'000'000; s.buildersSeen = s.published = 0; s.rejected = {};
}
inline void begin(const uint8_t *ram, uint32_t skeleton, uint32_t output) {
    auto &s = source(); s.builders.erase(output); s.pending.emplace_back();
    ++s.buildersSeen;
    if (!valid(skeleton, 8)) return;
    const unsigned count = word(ram, skeleton + 4) >> 16;
    // DCF48's signed bone byte addresses at most 128 bones.
    if (!count || count > 128 || !valid(skeleton, 8 + count * 16) || !valid(output, count * 64)) return;
    auto b = std::make_unique<Builder>(); b->output = output; b->skeleton = skeleton; b->bones.resize(count);
    for (unsigned i = 0; i < count; ++i) {
        if (!copy(ram, skeleton + 8 + i * 16, b->bones[i].bind)) return;
        const int parent = int16_t(b->bones[i].bind[3] & 0xFFFF);
        if (parent < -1 || parent >= int(i)) return;
    }
    s.pending.back() = std::move(b);
}
inline void end(const uint8_t *ram) {
    auto &s = source(); if (s.pending.empty()) return;
    auto b = std::move(s.pending.back()); s.pending.pop_back(); if (!b) return;
    for (unsigned i = 0; i < b->bones.size(); ++i)
        if (!copy(ram, b->output + i * 64, b->bones[i].matrix)) return;
    const auto output = b->output;
    s.builders[output] = std::shared_ptr<const Builder>(std::move(b));
}
inline std::shared_ptr<const Pose> pose(const uint8_t *ram, uint32_t list, uint32_t player, uint32_t model,
                                      std::shared_ptr<const Pose> cached = {}, uint32_t actor = 0) {
    if (!player || player > 8 || !valid(model, 64) || !valid(list, 0x50)) return reject(0);
    const uint32_t occupant = word(ram, 0x80135490U + (player - 1) * 4);
    if (!valid(occupant, 4) || (actor && actor != occupant)) return reject(0);
    actor = occupant;
    auto &s = source(); auto it = s.builders.find(list + 0x50); if (it == s.builders.end()) return reject(1);
    const auto &b = *it->second; const unsigned count = unsigned(b.bones.size());
    // B27E0 supplies model + model[0x18] as AE820's skeleton. Require the
    // actual latest builder and unchanged authored bytes, not a guessed model.
    const uint32_t offset = word(ram, model + 0x18);
    if (!offset || uint64_t(model) + offset != b.skeleton || word(ram, list) != count) return reject(2);
    for (unsigned i = 0; i < count; ++i) {
        std::array<uint32_t, 4> bind;
        Words result;
        if (!copy(ram, b.skeleton + 8 + i * 16, bind) || bind != b.bones[i].bind) return reject(2);
        if (!copy(ram, b.output + i * 64, result) || result != b.bones[i].matrix) return reject(3);
    }
    Words header, root;
    if (!copy(ram, model, header) || !copy(ram, 0x8012CE58, root)) return reject(4);
    if (cached && cached->frame == s.frame && cached->player == player && cached->actor == actor && cached->model == model &&
        cached->modelHeader == header && cached->root == root && cached->bones.size() == count) {
        bool equal = true;
        for (unsigned i = 0; i < count; ++i) equal &= cached->bones[i].bind == b.bones[i].bind && cached->bones[i].matrix == b.bones[i].matrix;
        if (equal) return cached;
    }
    auto value = std::make_shared<Pose>(); value->player = player; value->actor = actor; value->model = model; value->frame = s.frame;
    value->modelHeader = header; value->root = root; value->bones = b.bones; return value;
}
// Called after the game's fixed matrix and complete group have been emitted.
// No extra guest allocation or display-list space is used: V1's reserved word
// carries an opaque generation, never a native pointer or guest pose address.
inline uint32_t publish(const uint8_t *ram, uint32_t command, uint32_t matrixId, unsigned bone, std::shared_ptr<const Pose> pose) {
    if (!pose || bone >= pose->bones.size() || command < 0x80000008U || !valid(command, 16)) return 0;
    const uint32_t previous = command - 8;
    if (word(ram, previous) != 0xDA380002U) return 0;
    Binding b; b.pose = std::move(pose); b.bone = bone; b.matrixId = matrixId;
    b.address = word(ram, previous + 4) & 0x7FFFFF;
    if (!copy(ram, b.address | 0x80000000U, b.fixed)) return 0;
    const auto token = mailbox().publish(ram + (command & 0x7FFFFF), std::move(b));
    ++source().published; return token;
}
}
#endif
