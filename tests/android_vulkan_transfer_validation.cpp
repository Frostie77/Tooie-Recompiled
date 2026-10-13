// Executes the production Android clear code through a host Vulkan driver.
#include "plume_vulkan.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace plume;
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "FAILED %d: %s\n", __LINE__, #x);                   \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
extern "C" int32_t ANativeWindow_getWidth(ANativeWindow *) { return 1; }
extern "C" int32_t ANativeWindow_getHeight(ANativeWindow *) { return 1; }
static unsigned validationErrors = 0;
static VKAPI_ATTR VkBool32 VKAPI_CALL
report(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
       VkDebugUtilsMessageTypeFlagsEXT,
       const VkDebugUtilsMessengerCallbackDataEXT *data, void *) {
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    ++validationErrors;
    std::fprintf(stderr, "VALIDATION: %s\n", data->pMessage);
  }
  return VK_FALSE;
}
int main() {
  VulkanInterface
      interface; // Expected Android-surface rejection on a host loader.
  const char *layers[] = {"VK_LAYER_KHRONOS_validation"};
  const char *extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                              VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                              VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
  VkValidationFeatureEnableEXT enable =
      VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
  VkValidationFeaturesEXT features = {};
  features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
  features.enabledValidationFeatureCount = 1;
  features.pEnabledValidationFeatures = &enable;
  VkApplicationInfo app = {};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.apiVersion = VK_API_VERSION_1_2;
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create.pNext = &features;
  create.pApplicationInfo = &app;
  create.enabledLayerCount = 1;
  create.ppEnabledLayerNames = layers;
  create.enabledExtensionCount = 3;
  create.ppEnabledExtensionNames = extensions;
  REQUIRE(vkCreateInstance(&create, nullptr, &interface.instance) ==
          VK_SUCCESS);
  volkLoadInstance(interface.instance);
  VkDebugUtilsMessengerCreateInfoEXT debug = {};
  debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug.pfnUserCallback = report;
  VkDebugUtilsMessengerEXT messenger;
  REQUIRE(vkCreateDebugUtilsMessengerEXT(interface.instance, &debug, nullptr,
                                         &messenger) == VK_SUCCESS);
  {
    VulkanDevice device(&interface, "llvmpipe");
    REQUIRE(device.vk != VK_NULL_HANDLE);
    REQUIRE(device.physicalDeviceProperties.deviceType ==
            VK_PHYSICAL_DEVICE_TYPE_CPU);
    auto queue = device.createCommandQueue(RenderCommandListType::DIRECT);
    auto list = queue->createCommandList();
    auto fence = device.createCommandFence();
    std::printf("Vulkan validation device: %s\n",
                device.getDescription().name.c_str());
    for (uint32_t samples : {1u, 4u}) {
      REQUIRE(device.getSampleCountsSupported(RenderFormat::B8G8R8A8_UNORM) &
              samples);
      RenderMultisampling ms;
      ms.sampleCount = samples;
      auto target = device.createTexture(RenderTextureDesc::ColorTarget(
          16, 16, RenderFormat::B8G8R8A8_UNORM, ms));
      auto resolved = device.createTexture(
          RenderTextureDesc::ColorTarget(16, 16, RenderFormat::B8G8R8A8_UNORM));
      const RenderTexture *color = target.get();
      auto framebuffer =
          device.createFramebuffer(RenderFramebufferDesc(&color, 1));
      auto buffer =
          device.createBuffer(RenderBufferDesc::ReadbackBuffer(16 * 16 * 4));
      auto *vkBuffer = static_cast<VulkanBuffer *>(buffer.get());
      auto *vkList = static_cast<VulkanCommandList *>(list.get());
      for (unsigned iteration = 0; iteration < 2; ++iteration) {
        list->begin();
        list->barriers(RenderBarrierStage::GRAPHICS,
                       RenderTextureBarrier(target.get(),
                                            RenderTextureLayout::COLOR_WRITE));
        list->setFramebuffer(framebuffer.get());
        // Verify a nonzero color, then transparent black on the same image.
        list->clearColor(0, iteration == 0
                                ? RenderColor(0.25f, 0.5f, 0.75f, 0.0f)
                                : RenderColor(0.0f, 0.0f, 0.0f, 0.0f));
        RenderTexture *readSource = target.get();
        if (samples > 1) {
          RenderTextureBarrier transitions[] = {
              RenderTextureBarrier(target.get(),
                                   RenderTextureLayout::RESOLVE_SOURCE),
              RenderTextureBarrier(resolved.get(),
                                   RenderTextureLayout::RESOLVE_DEST)};
          list->barriers(RenderBarrierStage::COPY, transitions, 2);
          list->resolveTexture(resolved.get(), target.get());
          readSource = resolved.get();
        }
        list->barriers(
            RenderBarrierStage::COPY,
            RenderTextureBarrier(readSource, RenderTextureLayout::COPY_SOURCE));
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {16, 16, 1};
        vkCmdCopyImageToBuffer(
            vkList->vk, static_cast<VulkanTexture *>(readSource)->vk,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkBuffer->vk, 1, &region);
        VkBufferMemoryBarrier host = {};
        host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        host.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        host.buffer = vkBuffer->vk;
        host.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(vkList->vk, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1,
                             &host, 0, nullptr);
        list->end();
        const RenderCommandList *commands = list.get();
        queue->executeCommandLists(&commands, 1, nullptr, 0, nullptr, 0,
                                   fence.get());
        queue->waitForCommandFence(fence.get());
        REQUIRE(vmaInvalidateAllocation(device.allocator, vkBuffer->allocation,
                                        0, VK_WHOLE_SIZE) == VK_SUCCESS);
        const auto *bytes = static_cast<unsigned char *>(buffer->map());
        REQUIRE(bytes != nullptr);
        const int expected[] = {iteration == 0 ? 191 : 0,
                                iteration == 0 ? 128 : 0,
                                iteration == 0 ? 64 : 0, 0};
        for (unsigned pixel = 0; pixel < 16 * 16; ++pixel)
          for (unsigned channel = 0; channel < 4; ++channel)
            REQUIRE(std::abs(int(bytes[pixel * 4 + channel]) -
                             expected[channel]) <= 1);
        buffer->unmap();
      }
      std::printf("PASS: %ux transfer/load-op clear, resolve/readback, repeat, "
                  "transparent alpha\n",
                  samples);
    }
  }
  vkDestroyDebugUtilsMessengerEXT(interface.instance, messenger, nullptr);
  REQUIRE(validationErrors == 0);
  std::puts("PASS: no Vulkan core or synchronization validation errors");
}
