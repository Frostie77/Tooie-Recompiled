#pragma once
#include "../include/skeletal_pose_config.h"
#if TOOIE_SKELETAL_POSE_ENABLED
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace tooie::skeletal {
using Words = std::array<uint32_t, 16>;
struct Bone {
    std::array<uint32_t, 4> bind{}; // xyz floats; channel/parent halfwords
    Words matrix{};                // completed model-space builder output
};
struct Pose {
    uint32_t player = 0, actor = 0, model = 0;
    uint64_t actorGeneration = 0; // nonzero only for a registered actor marker
    uint64_t frame = 0;
    Words modelHeader{}, root{};
    std::vector<Bone> bones;
};
struct Binding {
    std::shared_ptr<const Pose> pose;
    Words fixed{};
    uint32_t address = 0, matrixId = 0, token = 0;
    unsigned bone = 0;
};
using BindingPtr = std::shared_ptr<const Binding>;
using TaskBindings = std::unordered_map<uintptr_t, BindingPtr>;

// Continuous producer -> actual queued task -> RSP ownership, independent of
// the sampled correction observer. No deadlines, try-locks or sample quotas.
// Keys are command-storage identities only; nobody dereferences them here.
// Reusing a guest command slot replaces its pending value. Queued tasks retain
// their own immutable copies, including when that slot is reused subsequently.
class Mailbox {
    std::mutex mutex;
    TaskBindings pending;
    uint32_t next = 0;
public:
    uint32_t publish(const void *command, Binding value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (next == UINT32_MAX) throw std::overflow_error("skeletal command generation exhausted");
        value.token = ++next;
        pending[reinterpret_cast<uintptr_t>(command)] = std::make_shared<const Binding>(std::move(value));
        return next;
    }
    std::shared_ptr<const TaskBindings> snapshot() {
        std::lock_guard<std::mutex> lock(mutex);
        return std::make_shared<const TaskBindings>(pending);
    }
    void retire(const TaskBindings &used) {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto &entry : used) {
            auto i = pending.find(entry.first);
            if (i != pending.end() && i->second == entry.second) pending.erase(i);
        }
    }
};
inline Mailbox &mailbox() { static Mailbox value; return value; }

// Scope lives around send_dl, on the graphics thread. RSP consumers copy shared
// ownership into groups/workloads before it is released. Nested scopes restore
// their predecessor; completion cannot retire a newer generation of a slot.
class TaskScope {
    std::shared_ptr<const TaskBindings> entries;
    TaskBindings used;
    TaskScope *previous;
    static TaskScope *&current() { static thread_local TaskScope *value = nullptr; return value; }
public:
    explicit TaskScope(std::shared_ptr<const TaskBindings> value) : entries(std::move(value)), previous(current()) { current() = this; }
    ~TaskScope() { mailbox().retire(used); current() = previous; }
    static BindingPtr find(const void *command, uint32_t token, uint32_t matrixId) {
        auto s = current();
        if (!s || !s->entries || !token) return {};
        const auto key = reinterpret_cast<uintptr_t>(command);
        auto i = s->entries->find(key);
        if (i == s->entries->end() || i->second->token != token || i->second->matrixId != matrixId) return {};
        s->used.emplace(*i);
        return i->second;
    }
};
}
#endif
