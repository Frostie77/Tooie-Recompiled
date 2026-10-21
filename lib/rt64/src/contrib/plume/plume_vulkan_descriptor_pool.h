#pragma once
#include <vulkan/vulkan_core.h>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace plume {
// Android-only caller. The raw driverVersion encoding was not supplied in the
// Odin evidence; do not invent a version cutoff from Logcat's "0676.53".
inline const char *androidDescriptorPoolQualification(const VkPhysicalDeviceProperties &device,
    const VkPhysicalDeviceDriverProperties &driver) {
    if (device.vendorID != 0x5143) return "different-vendor";
    if (driver.sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES) return "driver-properties-unavailable";
    if (driver.driverID != VK_DRIVER_ID_QUALCOMM_PROPRIETARY) return "different-driver";
    if (std::strcmp(device.deviceName, "Adreno (TM) 740") && std::strcmp(device.deviceName, "Adreno 740")) return "different-gpu";
    return "adreno-740-proprietary-declared-bound";
}

struct VulkanDescriptorPoolPlan {
    std::unordered_map<VkDescriptorType, uint32_t> capacities;
    uint64_t updateAfterBindSlots = 0;
    bool declarationReserved = false;
    const char *error = nullptr;
};

// Reservation and allocation are separate. This function cannot modify the
// caller's actual counts, layout, binding flags or allocation pNext.
inline VulkanDescriptorPoolPlan planAndroidDescriptorPool(const std::vector<VkDescriptorSetLayoutBinding> &bindings,
    const std::vector<VkDescriptorBindingFlags> &flags, const std::vector<uint32_t> &actual,
    bool qualified, bool updateAfterBind, uint32_t allPoolsLimit, uint64_t liveSlots) {
    VulkanDescriptorPoolPlan plan;
    if (bindings.size() != actual.size() || bindings.size() != flags.size()) {
        plan.error = "invalid-pool-plan-bindings"; return plan;
    }
    constexpr VkDescriptorBindingFlags required = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    for (size_t i = 0; i < bindings.size(); ++i) {
        const auto &b = bindings[i];
        if (!actual[i] || actual[i] > b.descriptorCount ||
            (!(flags[i] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) && actual[i] != b.descriptorCount)) {
            plan.error = "invalid-pool-plan-count"; return plan;
        }
        const bool reserve = qualified && updateAfterBind && b.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE &&
            (flags[i] & required) == required;
        const uint32_t count = reserve ? b.descriptorCount : actual[i];
        // Include fixed bindings of the same type and immutable samplers.
        const uint64_t total = uint64_t(plan.capacities[b.descriptorType]) + count;
        if (total > UINT32_MAX) { plan.error = "descriptor-pool-capacity-overflow"; return plan; }
        plan.capacities[b.descriptorType] = uint32_t(total);
        if (updateAfterBind) plan.updateAfterBindSlots += count;
        plan.declarationReserved |= reserve;
    }
    // This advertised slot limit is a validity limit, never a free-byte budget.
    if (updateAfterBind && (liveSlots > allPoolsLimit || plan.updateAfterBindSlots > allPoolsLimit - liveSlots))
        plan.error = "descriptor-pool-update-after-bind-limit";
    return plan;
}
}
