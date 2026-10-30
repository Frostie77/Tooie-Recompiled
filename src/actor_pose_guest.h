#pragma once
#include "skeletal_pose_guest.h"
#if TOOIE_ACTOR_POSE_ENABLED
#include <limits>

namespace tooie::skeletal::actors {
// Actor storage moves on vector growth and swap-with-last removal. The marker
// remains the actor's handle; its address alone is not a lifetime identity.
// Register only the actual constructor and invalidate on actual marker free.
struct Owner {
    uint32_t marker = 0, actor = 0, drawCallback = 0;
    uint64_t generation = 0;
    explicit operator bool() const { return marker && generation; }
};
class Registry {
    struct Entry { uint64_t generation; std::unordered_map<uint64_t, uint32_t> models; };
    std::mutex mutex;
    std::unordered_map<uint32_t, Entry> entries;
    uint64_t generation = 0;
    uint32_t nextModel = 0x20000000U;
public:
    void create(uint32_t marker) {
        std::lock_guard<std::mutex> lock(mutex);
        if (generation == UINT64_MAX) throw std::overflow_error("actor lifetime generation exhausted");
        entries.insert_or_assign(marker, Entry{++generation, {}});
    }
    void retire(uint32_t marker) { std::lock_guard<std::mutex> lock(mutex); entries.erase(marker); }
    void clear() { std::lock_guard<std::mutex> lock(mutex); entries.clear(); }
    uint64_t find(uint32_t marker) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = entries.find(marker); return it == entries.end() ? 0 : it->second.generation;
    }
    uint32_t model(const Owner &owner, uint32_t model) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = entries.find(owner.marker);
        if (it == entries.end() || it->second.generation != owner.generation) return 0;
        const uint64_t key = (uint64_t(owner.drawCallback) << 32) | model;
        auto old = it->second.models.find(key);
        if (old != it->second.models.end()) return old->second;
        // Disjoint from player, sky and reserved IDs. Never recycle an ID while
        // an older task/workload might still refer to it, including after reset.
        if (nextModel > 0xEFFFFF00U) throw std::overflow_error("actor transform identity exhausted");
        const auto id = nextModel; nextModel += 0x100U;
        it->second.models.emplace(key, id); return id;
    }
};
inline Registry &registry() { static Registry value; return value; }
inline uint16_t half(const uint8_t *ram, uint32_t p) {
    uint16_t v = 0;
    if (!(p & 1) && p >= 0x80000000U && p <= 0x807FFFFEU) std::memcpy(&v, ram + ((p & 0x7FFFFF) ^ 2), 2);
    return v;
}
inline uint32_t canonical(const uint8_t *ram, uint32_t marker) {
    if (!guest::valid(marker, 0x2C)) return 0;
    const uint32_t pool = guest::word(ram, 0x80136EE0);
    if (!guest::valid(pool, 12) || guest::word(ram, pool) != 0x9C) return 0;
    const uint32_t begin = guest::word(ram, pool + 4), end = guest::word(ram, pool + 8);
    if (end < begin || !guest::valid(begin, end - begin) || (end - begin) % 0x9C) return 0;
    const uint32_t index = half(ram, marker + 0x1A) >> 5;
    const uint64_t address = uint64_t(begin) + uint64_t(index) * 0x9C;
    if (address + 0x9C > end) return 0;
    const uint32_t actor = uint32_t(address);
    return guest::word(ram, actor) == marker ? actor : 0;
}
inline void created(const uint8_t *ram, uint32_t actor) {
    if (!guest::valid(actor, 0x9C)) return;
    const auto marker = guest::word(ram, actor);
    registry().retire(marker);
    if (canonical(ram, marker) == actor) registry().create(marker);
}
inline std::vector<Owner> &scopes() { static thread_local std::vector<Owner> value; return value; }
struct Stats { uint64_t next = 0, draws = 0, admitted = 0, roots = 0, poses = 0, unsupported = 0, details[2]{}; };
inline Stats &stats() { static thread_local Stats value; return value; }
inline void frameBegin() {
    scopes().clear();
    auto &s = stats(); const auto now = diagnostics::monotonicUs();
    if (now < s.next || !s.draws) return;
    char text[400]; std::snprintf(text, sizeof(text),
        "draws=%llu admitted=%llu roots=%llu skeletal_bones=%llu unsupported_bones=%llu identity=marker-constructor-generation array_stride=156",
        (unsigned long long)s.draws, (unsigned long long)s.admitted, (unsigned long long)s.roots,
        (unsigned long long)s.poses, (unsigned long long)s.unsupported);
    diagnostics::log("actor-source", text);
    s.draws = s.admitted = s.roots = s.poses = s.unsupported = 0; s.next = now + 5'000'000;
}
inline void begin(const uint8_t *ram, uint32_t actor, uint32_t drawCallback) {
    Owner owner;
    if (guest::valid(actor, 0x9C)) {
        owner.marker = guest::word(ram, actor);
        if (canonical(ram, owner.marker) == actor) {
            owner.actor = actor; owner.drawCallback = drawCallback; owner.generation = registry().find(owner.marker);
        }
    }
    ++stats().draws; stats().admitted += bool(owner);
    scopes().push_back(owner); // invalid nested draws cannot borrow their caller
}
inline void end() { if (!scopes().empty()) scopes().pop_back(); }
inline Owner current(const uint8_t *ram) {
    if (scopes().empty()) return {};
    const auto owner = scopes().back();
    if (!owner || canonical(ram, owner.marker) != owner.actor || registry().find(owner.marker) != owner.generation) return {};
    return owner;
}
inline void note(const uint8_t *ram, const Owner &owner, uint32_t model, uint32_t list, const char *result) {
    auto &s = stats(); const auto now = diagnostics::monotonicUs();
    auto &next = s.details[list != 0];
    if (now < next) return;
    const auto &source = guest::source();
    char text[600]; std::snprintf(text, sizeof(text),
        "marker=0x%X generation=%llu actor=0x%X type=%u callback=0x%X model=0x%X list=0x%X bones=%u result=%s validation=%s builder_skeleton=0x%X builder_bones=%u model_skeleton_offset=0x%X",
        owner.marker, (unsigned long long)owner.generation, owner.actor, unsigned(half(ram, owner.marker + 0x14)),
        owner.drawCallback, model, list, guest::word(ram, list), result,
        list ? source.lastRejection : "none", list ? source.lastBuilderSkeleton : 0,
        list ? source.lastBuilderBones : 0, list && guest::valid(model, 64) ? guest::word(ram, model + 0x18) : 0);
    diagnostics::log("actor-draw", text); next = now + 5'000'000;
}
inline std::shared_ptr<const Pose> pose(const uint8_t *ram, uint32_t list, uint32_t model,
                                       const Owner &owner, std::shared_ptr<const Pose> cached = {}) {
    const auto now = current(ram);
    if (!now || now.marker != owner.marker || now.generation != owner.generation) return guest::reject(0);
    auto value = guest::pose(ram, list, 0, model, std::move(cached), owner.marker, owner.generation);
    if (value) ++stats().poses; else ++stats().unsupported;
    note(ram, owner, model, list, value ? "owned-hierarchy" : "unsupported-current-pose");
    return value;
}
}
#endif
