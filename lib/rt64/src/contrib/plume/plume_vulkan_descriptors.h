#pragma once
// Android descriptor resources only. The owning device serializes validation /
// vkUpdateDescriptorSets against destruction; generations catch reused parent
// addresses. This does not replace caller GPU fences or descriptor ownership.
#include <mutex>
#include <unordered_map>
#include <cstdint>
namespace plume {
struct VulkanDescriptorResources {
    struct Entry {
        VkObjectType type;
        uint64_t handle, generation;
        const void *parent;
        uint64_t parentGeneration;
    };
    std::recursive_mutex mutex;
    std::unordered_map<const void *, Entry> objects;
    uint64_t nextGeneration = 1;
    void track(const void *object, VkObjectType type, uint64_t handle, const void *parent = nullptr) {
        const std::lock_guard lock(mutex);
        auto old = objects.find(object);
        const uint64_t generation = old != objects.end() && old->second.type == type && old->second.handle == handle
            ? old->second.generation : nextGeneration++;
        auto dependency = objects.find(parent);
        objects.insert_or_assign(object, Entry{type, handle, generation, parent,
            dependency != objects.end() ? dependency->second.generation : 0});
    }
    void erase(const void *object) { const std::lock_guard lock(mutex); objects.erase(object); }
    bool live(const void *object, VkObjectType type) const {
        auto it = objects.find(object);
        if (it == objects.end() || it->second.type != type) return false;
        if (!it->second.parent) return true;
        auto parent = objects.find(it->second.parent);
        return parent != objects.end() && parent->second.handle && parent->second.generation == it->second.parentGeneration;
    }
};
}
