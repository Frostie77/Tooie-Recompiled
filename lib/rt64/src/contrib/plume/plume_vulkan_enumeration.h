#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>
#include <utility>

namespace plume {
// Do not consume a partial list when a driver's count changes between calls.
template<class T, class Enumerate>
bool enumerateVulkanProperties(Enumerate enumerate, std::vector<T>& values, VkResult& result, unsigned& attempts) {
    values.clear();
    for (attempts = 1; attempts <= 8; ++attempts) {
        uint32_t count = 0;
        result = enumerate(&count, static_cast<T*>(nullptr));
        if (result != VK_SUCCESS && result != VK_INCOMPLETE) return false;
        if (count > 4096) { result = VK_ERROR_OUT_OF_HOST_MEMORY; return false; }
        if (!count) {
            if (result == VK_SUCCESS) return true;
            continue;
        }
        std::vector<T> next(count);
        auto written = count;
        result = enumerate(&written, next.data());
        if (result == VK_INCOMPLETE) continue;
        if (result != VK_SUCCESS) return false;
        if (written > count) { result = VK_ERROR_UNKNOWN; return false; }
        next.resize(written);
        values = std::move(next);
        return true;
    }
    attempts = 8;
    result = VK_INCOMPLETE;
    return false;
}
}
