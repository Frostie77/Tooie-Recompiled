#pragma once
#include "plume_vulkan.h"
#include "../../../../../src/android_debug_log.h"
#include <cstdio>
#include <unistd.h>

namespace plume {
// Allocation-only experiment, never a rendering fallback. Each case owns fresh
// local structures and a separate layout/pool, destroyed before the next case.
inline bool runVulkanDescriptorAllocationProbe(VkDevice device,
    const VkPhysicalDeviceDescriptorIndexingFeatures &features, uint32_t upperBound = 8192) {
    struct Case { const char *name; uint32_t declared, capacity, allocated; bool variable; };
    const Case cases[] = {
        {"fixed-one", 1, 1, 1, false},
        {"variable-one", 1, 1, 1, true},
        {"production", upperBound, 1, 1, true},
        {"declared-pool", upperBound, upperBound, 1, true},
        {"full-variable", upperBound, upperBound, upperBound, true}
    };
    bool allSucceeded = true;
    for (const auto &test : cases) {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
        char record[768];
        auto log = [&](const char *phase, VkResult result, bool returned = false) {
            std::snprintf(record, sizeof(record), "Descriptor allocation probe: pid=%d tid=%d case=%s phase=%s declared=%u capacity=%u allocated=%u variable=%u returned=%u result=%d device=%llx layout=%llx pool=%llx set=%llx",
                int(getpid()), int(gettid()), test.name, phase, test.declared, test.capacity, test.allocated, unsigned(test.variable), unsigned(returned), int(result),
                (unsigned long long)device, (unsigned long long)layout, (unsigned long long)pool, (unsigned long long)set);
            tooie_android_debug_log(record); std::fprintf(stderr, "%s\n", record);
        };
        if (test.variable && (!features.descriptorBindingVariableDescriptorCount || !features.descriptorBindingPartiallyBound ||
            !features.runtimeDescriptorArray || !features.descriptorBindingSampledImageUpdateAfterBind)) {
            log("features-not-enabled", VK_NOT_READY); allSucceeded = false; continue;
        }
        VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0; binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        binding.descriptorCount = test.declared; binding.stageFlags = VK_SHADER_STAGE_ALL;
        VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo = {};
        flagInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagInfo.bindingCount = 1; flagInfo.pBindingFlags = &flags;
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1; layoutInfo.pBindings = &binding;
        layoutInfo.flags = test.variable ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;
        layoutInfo.pNext = test.variable ? &flagInfo : nullptr;
        VkDescriptorSetVariableDescriptorCountLayoutSupport variableSupport = {};
        variableSupport.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT;
        VkDescriptorSetLayoutSupport support = {};
        support.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT;
        support.pNext = test.variable ? &variableSupport : nullptr;
        vkGetDescriptorSetLayoutSupport(device, &layoutInfo, &support);
        if (!support.supported || (test.variable && variableSupport.maxVariableDescriptorCount < test.declared)) {
            log("layout-not-supported", VK_NOT_READY); allSucceeded = false; continue;
        }
        VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout);
        log("layout-create", result, true);
        if (result != VK_SUCCESS || !layout) { if (result == VK_ERROR_DEVICE_LOST) return false; allSucceeded = false; continue; }
        VkDescriptorPoolSize size = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, test.capacity };
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = test.variable ? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT : 0;
        poolInfo.maxSets = 1; poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &size;
        result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
        log("pool-create", result, true);
        if (result == VK_SUCCESS && pool) {
            VkDescriptorSetVariableDescriptorCountAllocateInfo variable = {};
            variable.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            variable.descriptorSetCount = 1; variable.pDescriptorCounts = &test.allocated;
            VkDescriptorSetAllocateInfo allocate = {};
            allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocate.descriptorPool = pool; allocate.descriptorSetCount = 1; allocate.pSetLayouts = &layout;
            allocate.pNext = test.variable ? &variable : nullptr;
            log("allocate-begin", VK_NOT_READY);
            result = vkAllocateDescriptorSets(device, &allocate, &set);
            log("allocate-return", result, true);
            if (result != VK_SUCCESS || !set) allSucceeded = false;
            // A failed allocation output is unusable. Destroy the valid pool,
            // which also releases any successfully allocated set; never free set.
            vkDestroyDescriptorPool(device, pool, nullptr);
        }
        else allSucceeded = false;
        vkDestroyDescriptorSetLayout(device, layout, nullptr);
        log("retired", VK_NOT_READY);
        if (result == VK_ERROR_DEVICE_LOST) return false;
    }
    return allSucceeded;
}
}
