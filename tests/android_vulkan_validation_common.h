// Shared CPU Vulkan fixture. Tests still exercise production Plume commands.
#pragma once
#include "plume_vulkan.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace plume;
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "FAILED %d: %s\n", __LINE__, #x);                   \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
extern "C" void ANativeWindow_acquire(ANativeWindow *) {}
extern "C" void ANativeWindow_release(ANativeWindow *) {}
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
// Keep a CPU expected image independent of the shader/recording implementation.
static std::vector<unsigned char>
readImage(VulkanDevice &device, RenderCommandList *list,
          RenderCommandQueue *queue, RenderCommandFence *fence,
          RenderTexture *texture, uint32_t width, uint32_t height,
          unsigned stride, uint32_t mip = 0, uint32_t layer = 0) {
  auto buffer = device.createBuffer(
      RenderBufferDesc::ReadbackBuffer(width * height * stride));
  auto *vkBuffer = static_cast<VulkanBuffer *>(buffer.get());
  auto *vkList = static_cast<VulkanCommandList *>(list);
  list->barriers(
      RenderBarrierStage::COPY,
      RenderTextureBarrier(texture, RenderTextureLayout::COPY_SOURCE));
  VkBufferImageCopy region = {};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, layer, 1};
  region.imageExtent = {width, height, 1};
  vkCmdCopyImageToBuffer(vkList->vk, static_cast<VulkanTexture *>(texture)->vk,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkBuffer->vk, 1,
                         &region);
  VkBufferMemoryBarrier host = {};
  host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  host.buffer = vkBuffer->vk;
  host.size = VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(vkList->vk, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &host, 0,
                       nullptr);
  list->end();
  const RenderCommandList *commands = list;
  queue->executeCommandLists(&commands, 1, nullptr, 0, nullptr, 0, fence);
  queue->waitForCommandFence(fence);
  REQUIRE(vmaInvalidateAllocation(device.allocator, vkBuffer->allocation, 0,
                                  VK_WHOLE_SIZE) == VK_SUCCESS);
  const auto *bytes = static_cast<const unsigned char *>(buffer->map());
  REQUIRE(bytes);
  std::vector<unsigned char> pixels(bytes, bytes + width * height * stride);
  buffer->unmap();
  return pixels;
}
static unsigned pixelStride(RenderFormat format) {
  return format == RenderFormat::R32G32B32A32_FLOAT ? 16
         : format == RenderFormat::R16G16B16A16_UNORM ||
                 format == RenderFormat::R16G16B16A16_FLOAT
             ? 8
             : 4;
}
static void expectPixels(const std::vector<unsigned char> &bytes,
                         const std::vector<RenderColor> &expected,
                         RenderFormat format) {
  const unsigned stride = pixelStride(format);
  const bool bgra = format == RenderFormat::B8G8R8A8_UNORM;
  REQUIRE(bytes.size() == expected.size() * stride);
  for (size_t pixel = 0; pixel < expected.size(); ++pixel) {
    for (unsigned channel = 0; channel < 4; ++channel) {
      const unsigned logical = bgra && channel != 3 ? 2 - channel : channel;
      float value = expected[pixel].rgba[logical];
      if (stride == 4) {
        if (std::abs(int(bytes[pixel * stride + channel]) -
                     int(std::round(value * 255))) > 1)
          std::fprintf(stderr,
                       "pixel=%zu channel=%u actual=%u expected=%g format=%u\n",
                       pixel, channel, bytes[pixel * stride + channel],
                       value * 255, unsigned(format));
        REQUIRE(std::abs(int(bytes[pixel * stride + channel]) -
                         int(std::round(value * 255))) <= 1);
      } else if (format == RenderFormat::R16G16B16A16_UNORM) {
        uint16_t word;
        std::memcpy(&word, &bytes[pixel * stride + channel * 2], 2);
        REQUIRE(std::abs(int(word) - int(std::round(value * 65535))) <= 1);
      } else if (format == RenderFormat::R16G16B16A16_FLOAT) {
        uint16_t word;
        std::memcpy(&word, &bytes[pixel * stride + channel * 2], 2);
        float actual = (word & 0x7c00)
                           ? std::ldexp(1.0f + (word & 1023) / 1024.0f,
                                        int((word >> 10) & 31) - 15)
                           : std::ldexp(float(word & 1023), -24);
        if (word & 0x8000)
          actual = -actual;
        REQUIRE(std::abs(actual - value) < 0.001f);
      } else {
        float actual;
        std::memcpy(&actual, &bytes[pixel * stride + channel * 4], 4);
        REQUIRE(std::abs(actual - value) < 0.0001f);
      }
    }
  }
}
static void paint(std::vector<RenderColor> &pixels, unsigned width,
                  RenderRect rectangle, RenderColor color) {
  for (int y = rectangle.top; y < rectangle.bottom; ++y)
    for (int x = rectangle.left; x < rectangle.right; ++x)
      pixels[y * width + x] = color;
}

struct HostVulkanValidation {
  VulkanInterface interface;
  VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
  HostVulkanValidation() {
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
    REQUIRE(vkCreateDebugUtilsMessengerEXT(interface.instance, &debug, nullptr,
                                           &messenger) == VK_SUCCESS);
  }
  ~HostVulkanValidation() {
    vkDestroyDebugUtilsMessengerEXT(interface.instance, messenger, nullptr);
    REQUIRE(validationErrors == 0);
    std::puts("PASS: no Vulkan core or synchronization validation errors");
  }
};
