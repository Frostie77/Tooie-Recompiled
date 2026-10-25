        // Exact block from main 6dc376a95fee, plume_vulkan.cpp blob ba28d171e422aa8d9e7cb2748ee4df0026e7d68a.
        // Check for extensions.
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

