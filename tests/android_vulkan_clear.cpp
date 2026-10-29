// Recording tests of production Android code, not GPU execution. Synthetic
// images describe the launcher's state; Vulkan entry points check every
// command.
#include "../lib/RecompFrontend/recompui/src/renderer/ui_presentation.h"
#include "../lib/rt64/src/hle/rt64_vulkan_queue_diagnostic.h"
#include "../lib/rt64/src/render/rt64_vi_pipeline.h"
#include "plume_vulkan.h"
#include "../src/android_presentation_lifecycle.h"
#include "../lib/rt64/src/hle/rt64_android_surface.h"
#include "plume_vulkan_clear_shaders.h"
#include "hud_capture_fixture.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>
using namespace plume;
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #x);              \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
static std::unordered_map<ANativeWindow *, int> windowReferences;
extern "C" void ANativeWindow_acquire(ANativeWindow *window) { ++windowReferences[window]; }
extern "C" void ANativeWindow_release(ANativeWindow *window) { REQUIRE(windowReferences[window] > 0); --windowReferences[window]; }
extern "C" int32_t ANativeWindow_getWidth(ANativeWindow *) { return 640; }
extern "C" int32_t ANativeWindow_getHeight(ANativeWindow *) { return 480; }
template <class T> T handle() {
  static size_t id = 1;
  return reinterpret_cast<T>(id++);
}
struct PassRecord {
  std::vector<VkAttachmentDescription> attachments;
  std::vector<VkAttachmentReference> colors;
  VkAttachmentReference depth = {VK_ATTACHMENT_UNUSED,
                                 VK_IMAGE_LAYOUT_UNDEFINED};
};
static std::unordered_map<VkRenderPass, PassRecord> passes;
static std::unordered_map<VkFramebuffer, VkRenderPass> framebufferPasses;
static VkExtent2D expectedFramebufferExtent = {640, 480};
static std::unordered_map<VkFramebuffer, VkExtent2D> framebufferExtents;
static std::unordered_map<VkImage, VkImageLayout> imageLayouts;
static std::vector<char> commands;
static bool isolation = false, acquireFails = false, colorFeature = true;
static unsigned isolationSubmits = 0, isolationWaits = 0, acquireCalls = 0;
static VkSemaphore acquired = VK_NULL_HANDLE;
static VkPipeline boundPipeline = VK_NULL_HANDLE;
static std::unordered_set<VkPipeline> clearPipelines;
static std::vector<VkViewport> boundViewports;
static std::vector<VkRect2D> boundScissors, clearDrawScissors;
static float boundDepthBias[3] = {};
static std::vector<uint8_t> pushed;
static unsigned clearDraws = 0, drawPipelineCreates = 0;
static VkRenderPass selectedPass;
static VkClearColorValue selectedValue;
static VkImage selectedImage;
static bool inPass = false, simulateTimeout = false, invalidState = false;
static bool depthClearRecording = false;
static std::vector<VkClearRect> recordedDepthRects;
static VkClearAttachment recordedDepthAttachment{};
static bool transferFeature = true;
static const char *clearRejectionCase = nullptr;
static bool rejectBeforeRecording = false;
static VkImageCreateInfo expectedImageInfo = {};
static VkImageFormatProperties imageLimits = {};
static VkResult imageQueryResult = VK_SUCCESS;
static unsigned imageQueries = 0, swapchainCreates = 0;
static VkResult swapchainCreateResult = VK_SUCCESS, swapchainImageCountResult = VK_SUCCESS, acquireResult = VK_SUCCESS;
static bool viewCreateFails = false;
static int imageDataCount = -1;
static bool lifecycleRecording = false, idleRetired = false;
static std::unordered_map<VkFramebuffer, std::vector<VkImageView>> framebufferViews;
static std::vector<char> lifecycleOrder;
static VkResult queueIdle(VkQueue) { idleRetired = true; lifecycleOrder.push_back('Q'); return VK_SUCCESS; }
static VkResult queuePresent(VkQueue, const VkPresentInfoKHR *) { idleRetired = false; return VK_SUCCESS; }
static VkImageUsageFlags surfaceUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT;
static VkResult surfaceQueryResult = VK_SUCCESS,
                surfaceCreateResult = VK_SUCCESS;
static VkResult supportQueryResult = VK_SUCCESS, formatCountResult = VK_SUCCESS,
                formatDataResult = VK_SUCCESS;
static VkResult modeCountResult = VK_SUCCESS, modeDataResult = VK_SUCCESS;
static VkBool32 queuePresentSupport = VK_TRUE;
static VkCompositeAlphaFlagsKHR surfaceAlpha =
    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
static std::vector<VkSurfaceFormatKHR> reportedSurfaceFormats = {
    {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
static bool formatsGrowOnce = false, formatsAlwaysIncomplete = false,
            formatsShrink = false;
static bool formatsOverreported = false, formatsZeroWritten = false;
static unsigned formatCountQueries = 0, formatDataQueries = 0,
                capabilitiesQueries = 0;
static VkFormat createdSwapchainFormat = VK_FORMAT_UNDEFINED;
static VkFormat expectedSwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
static VkFormatFeatureFlags presentationFeatures =
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
static bool swapImageConfigSupported = true;
static unsigned swapImageQueries = 0;
static bool presentationPipelines = false;
struct PresentationPipelineRecord {
  VkFormat format;
  VkSampleCountFlagBits samples;
  VkPipelineColorBlendAttachmentState blend;
};
static std::vector<PresentationPipelineRecord> presentationRecords;
static std::unordered_map<VkImage, VkFormat> swapImageFormats;
static std::unordered_map<VkImageView, VkFormat> actualViewFormats;
static unsigned createdPasses = 0, destroyedPasses = 0, selectedIndex = 0;
static VkResult enumerate(VkInstance, uint32_t *count, VkPhysicalDevice *) {
  *count = 0;
  return VK_SUCCESS;
}
static void formatProperties(VkPhysicalDevice, VkFormat,
                             VkFormatProperties *out) {
  *out = {};
  out->optimalTilingFeatures =
      (transferFeature ? VK_FORMAT_FEATURE_TRANSFER_DST_BIT : 0) |
      (colorFeature ? VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT : 0) |
      presentationFeatures;
}
static VkResult imageProperties(VkPhysicalDevice, VkFormat format,
                                VkImageType type, VkImageTiling tiling,
                                VkImageUsageFlags usage,
                                VkImageCreateFlags flags,
                                VkImageFormatProperties *out) {
  if (usage == (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) {
    ++swapImageQueries;
    REQUIRE(format == expectedSwapchainFormat && type == VK_IMAGE_TYPE_2D &&
            tiling == VK_IMAGE_TILING_OPTIMAL && flags == 0);
    *out = {};
    out->maxExtent = {4096, 4096, 1};
    out->maxMipLevels = out->maxArrayLayers = 1;
    out->sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    return swapImageConfigSupported ? VK_SUCCESS
                                    : VK_ERROR_FORMAT_NOT_SUPPORTED;
  }
  ++imageQueries;
  REQUIRE(format == expectedImageInfo.format &&
          type == expectedImageInfo.imageType);
  REQUIRE(tiling == expectedImageInfo.tiling &&
          usage == expectedImageInfo.usage && flags == expectedImageInfo.flags);
  *out = imageLimits;
  return imageQueryResult;
}
static VkResult createSurface(VkInstance, const VkAndroidSurfaceCreateInfoKHR *,
                              const VkAllocationCallbacks *,
                              VkSurfaceKHR *out) {
  *out = surfaceCreateResult == VK_SUCCESS ? handle<VkSurfaceKHR>()
                                           : VK_NULL_HANDLE;
  return surfaceCreateResult;
}
static void destroySurface(VkInstance, VkSurfaceKHR,
                           const VkAllocationCallbacks *) { if (lifecycleRecording) lifecycleOrder.push_back('S'); }
static VkResult presentSupport(VkPhysicalDevice, uint32_t, VkSurfaceKHR,
                               VkBool32 *out) {
  *out = queuePresentSupport;
  return supportQueryResult;
}
static VkResult surfaceCaps(VkPhysicalDevice, VkSurfaceKHR,
                            VkSurfaceCapabilitiesKHR *out) {
  ++capabilitiesQueries;
  *out = {};
  out->minImageCount = 2;
  out->maxImageCount = 3;
  out->currentExtent = {640, 480};
  out->minImageExtent = {16, 16};
  out->maxImageExtent = {4096, 4096};
  out->maxImageArrayLayers = 1;
  out->supportedCompositeAlpha = surfaceAlpha;
  out->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  out->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  out->supportedUsageFlags = surfaceUsage;
  return surfaceQueryResult;
}
static VkResult surfaceFormats(VkPhysicalDevice, VkSurfaceKHR, uint32_t *count,
                               VkSurfaceFormatKHR *out) {
  if (!out) {
    ++formatCountQueries;
    *count = uint32_t(reportedSurfaceFormats.size());
    if (formatsGrowOnce && formatCountQueries == 1)
      --*count;
    return formatCountResult;
  }
  ++formatDataQueries;
  const unsigned capacity = *count;
  const unsigned written =
      formatsZeroWritten
          ? 0
          : std::min(capacity, unsigned(reportedSurfaceFormats.size()) -
                                   unsigned(formatsShrink));
  for (unsigned i = 0; i < written; ++i)
    out[i] = reportedSurfaceFormats[i];
  *count = formatsOverreported ? capacity + 1 : written;
  if (formatsAlwaysIncomplete || (formatsGrowOnce && formatDataQueries == 1))
    return VK_INCOMPLETE;
  return formatDataResult;
}
static VkResult presentModes(VkPhysicalDevice, VkSurfaceKHR, uint32_t *count,
                             VkPresentModeKHR *out) {
  *count = 1;
  if (out)
    *out = VK_PRESENT_MODE_FIFO_KHR;
  return out ? modeDataResult : modeCountResult;
}
static VkResult createSwapchain(VkDevice, const VkSwapchainCreateInfoKHR *p,
                                const VkAllocationCallbacks *,
                                VkSwapchainKHR *out) {
  ++swapchainCreates;
  REQUIRE(p->imageFormat != VK_FORMAT_UNDEFINED &&
          p->imageFormat == expectedSwapchainFormat);
  createdSwapchainFormat = p->imageFormat;
  REQUIRE(p->minImageCount >= 2);
  REQUIRE((p->imageUsage & ~surfaceUsage) == 0);
  REQUIRE(p->imageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  *out = swapchainCreateResult == VK_SUCCESS ? handle<VkSwapchainKHR>() : VK_NULL_HANDLE;
  return swapchainCreateResult;
}
static void destroySwapchain(VkDevice, VkSwapchainKHR,
                             const VkAllocationCallbacks *) { if (lifecycleRecording) { REQUIRE(idleRetired); lifecycleOrder.push_back('W'); } }
static VkResult swapchainImages(VkDevice, VkSwapchainKHR, uint32_t *count,
                                VkImage *out) {
  *count = 2;
  if (!out && swapchainImageCountResult != VK_SUCCESS) return swapchainImageCountResult;
  if (out) {
    out[0] = handle<VkImage>();
    out[1] = handle<VkImage>();
    imageLayouts[out[0]] = imageLayouts[out[1]] = VK_IMAGE_LAYOUT_UNDEFINED;
    swapImageFormats[out[0]] = swapImageFormats[out[1]] =
        createdSwapchainFormat;
    if (imageDataCount >= 0) *count = unsigned(imageDataCount);
  }
  return VK_SUCCESS;
}
static VkResult acquireImage(VkDevice, VkSwapchainKHR, uint64_t timeout,
                             VkSemaphore semaphore, VkFence, uint32_t *out) {
  REQUIRE(timeout == 100000000ULL);
  ++acquireCalls;
  acquired = semaphore;
  *out = 0;
  return acquireFails ? VK_ERROR_OUT_OF_DATE_KHR : acquireResult;
}
static void testImageConfigurations(VulkanDevice &device) {
  expectedImageInfo = {};
  expectedImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  expectedImageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
  expectedImageInfo.imageType = VK_IMAGE_TYPE_2D;
  expectedImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  expectedImageInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  expectedImageInfo.samples = VK_SAMPLE_COUNT_8_BIT;
  expectedImageInfo.extent = {640, 480, 1};
  expectedImageInfo.mipLevels = 1;
  expectedImageInfo.arrayLayers = 1;
  imageLimits.maxExtent = {4096, 4096, 1};
  imageLimits.maxMipLevels = 12;
  imageLimits.maxArrayLayers = 256;
  imageLimits.sampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_8_BIT;
  const auto saved = expectedImageInfo;
  REQUIRE(
      VulkanTexture::validateImageConfiguration(&device, expectedImageInfo));
  // Query input forwarding includes tiling, STORAGE usage, and creation flags.
  expectedImageInfo.tiling = VK_IMAGE_TILING_LINEAR;
  expectedImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  expectedImageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  expectedImageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
  REQUIRE(
      VulkanTexture::validateImageConfiguration(&device, expectedImageInfo));
  expectedImageInfo = saved;
  imageQueryResult = VK_ERROR_FORMAT_NOT_SUPPORTED;
  REQUIRE(
      !VulkanTexture::validateImageConfiguration(&device, expectedImageInfo));
  imageQueryResult = VK_SUCCESS;
  imageLimits.sampleCounts = VK_SAMPLE_COUNT_1_BIT;
  REQUIRE(
      !VulkanTexture::validateImageConfiguration(&device, expectedImageInfo));
  imageLimits.sampleCounts |= VK_SAMPLE_COUNT_8_BIT;
  expectedImageInfo.extent.width = 4097;
  REQUIRE(
      !VulkanTexture::validateImageConfiguration(&device, expectedImageInfo));
  expectedImageInfo = saved;
  expectedImageInfo.mipLevels = 13;
  REQUIRE(
      !VulkanTexture::validateImageConfiguration(&device, expectedImageInfo));
  expectedImageInfo = saved;
  expectedImageInfo.arrayLayers = 257;
  REQUIRE(
      !VulkanTexture::validateImageConfiguration(&device, expectedImageInfo));
  expectedImageInfo = saved;
  REQUIRE(imageQueries == 7);
}
static void testSwapchainUsage(VulkanCommandQueue &queue) {
  RenderSwapChainDesc desc;
  desc.renderWindow = reinterpret_cast<ANativeWindow *>(1);
  desc.textureCount = 2;
  desc.format = RenderFormat::B8G8R8A8_UNORM;
  VulkanSwapChain swapchain(&queue, desc);
  const VkImageUsageFlags required = surfaceUsage;
  for (auto missing :
       {VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT}) {
    surfaceUsage = required & ~missing;
    REQUIRE(!swapchain.resize() && swapchainCreates == 0);
  }
  surfaceUsage = required;
  surfaceQueryResult = VK_ERROR_SURFACE_LOST_KHR;
  REQUIRE(!swapchain.resize() && swapchainCreates == 0);
  surfaceQueryResult = VK_SUCCESS;
  REQUIRE(swapchain.isSurfaceSuspended());
  swapchain.waitForIdle();
  swapchain.suspendSurface();
  REQUIRE(swapchain.resumeSurface(desc.renderWindow));
  REQUIRE(swapchain.resize() && swapchainCreates == 1);
  REQUIRE(swapchain.textures[0].imageUsage == required &&
          swapchain.textures[0].isSwapChainImage);
  const auto oldView = swapchain.textures[0].imageView;
  surfaceUsage &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  REQUIRE(!swapchain.resize() && swapchainCreates == 1 &&
          swapchain.textures[0].imageView == oldView);
  surfaceUsage = required;
}
static VkResult createPass(VkDevice, const VkRenderPassCreateInfo *p,
                           const VkAllocationCallbacks *, VkRenderPass *out) {
  REQUIRE(!rejectBeforeRecording);
  REQUIRE(p->subpassCount == 1 && p->dependencyCount == 0);
  *out = handle<VkRenderPass>();
  ++createdPasses;
  auto &record = passes[*out];
  record.attachments.assign(p->pAttachments,
                            p->pAttachments + p->attachmentCount);
  const auto &subpass = p->pSubpasses[0];
  REQUIRE(subpass.pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS);
  record.colors.assign(subpass.pColorAttachments,
                       subpass.pColorAttachments +
                           subpass.colorAttachmentCount);
  if (subpass.pDepthStencilAttachment)
    record.depth = *subpass.pDepthStencilAttachment;
  return VK_SUCCESS;
}
static void destroyPass(VkDevice, VkRenderPass pass,
                        const VkAllocationCallbacks *) {
  REQUIRE(passes.erase(pass) == 1);
  ++destroyedPasses;
}
static VkResult createFb(VkDevice, const VkFramebufferCreateInfo *p,
                         const VkAllocationCallbacks *, VkFramebuffer *out) {
  REQUIRE(p->width == expectedFramebufferExtent.width &&
          p->height == expectedFramebufferExtent.height && p->layers == 1);
  REQUIRE(p->attachmentCount == passes.at(p->renderPass).attachments.size());
  *out = handle<VkFramebuffer>();
  framebufferExtents[*out] = {p->width, p->height};
  framebufferPasses[*out] = p->renderPass;
  framebufferViews[*out] = std::vector<VkImageView>(p->pAttachments, p->pAttachments + p->attachmentCount);
  const auto &attachments = passes.at(p->renderPass).attachments;
  for (unsigned i = 0; i < p->attachmentCount; ++i)
    if (actualViewFormats.count(p->pAttachments[i]))
      REQUIRE(actualViewFormats.at(p->pAttachments[i]) ==
              attachments.at(i).format);
  return VK_SUCCESS;
}
static void destroyFb(VkDevice, VkFramebuffer fb,
                      const VkAllocationCallbacks *) {
  if (lifecycleRecording) { REQUIRE(idleRetired); lifecycleOrder.push_back('F'); }
  framebufferViews.erase(fb);
  REQUIRE(framebufferExtents.erase(fb) == 1);
  framebufferPasses.erase(fb);
}
static VkResult createView(VkDevice, const VkImageViewCreateInfo *info,
                           const VkAllocationCallbacks *, VkImageView *out) {
  REQUIRE(info->format != VK_FORMAT_UNDEFINED);
  if (swapImageFormats.count(info->image))
    REQUIRE(info->format == swapImageFormats.at(info->image));
  if (viewCreateFails) { *out = VK_NULL_HANDLE; return VK_ERROR_OUT_OF_DEVICE_MEMORY; }
  *out = handle<VkImageView>();
  actualViewFormats[*out] = info->format;
  return VK_SUCCESS;
}
static void destroyView(VkDevice, VkImageView view, const VkAllocationCallbacks *) {
  for (const auto &entry : framebufferViews)
    for (auto attachment : entry.second) REQUIRE(attachment != view);
  if (lifecycleRecording) { REQUIRE(idleRetired); lifecycleOrder.push_back('V'); }
}
static VkResult createPool(VkDevice, const VkCommandPoolCreateInfo *,
                           const VkAllocationCallbacks *, VkCommandPool *out) {
  *out = handle<VkCommandPool>();
  return VK_SUCCESS;
}
static void destroyPool(VkDevice, VkCommandPool,
                        const VkAllocationCallbacks *) {}
static VkResult allocate(VkDevice, const VkCommandBufferAllocateInfo *,
                         VkCommandBuffer *out) {
  *out = handle<VkCommandBuffer>();
  return VK_SUCCESS;
}
static void freeBuffers(VkDevice, VkCommandPool, uint32_t,
                        const VkCommandBuffer *) {}
static void beginPass(VkCommandBuffer, const VkRenderPassBeginInfo *p,
                      VkSubpassContents) {
  REQUIRE(!rejectBeforeRecording);
  REQUIRE(!inPass);
  inPass = true;
  commands.push_back('B');
  selectedPass = p->renderPass;
  const auto &base = passes.at(framebufferPasses.at(p->framebuffer));
  const auto &current = passes.at(p->renderPass);
  REQUIRE(base.attachments.size() == current.attachments.size());
  REQUIRE(base.colors.size() == current.colors.size());
  REQUIRE(base.depth.attachment == current.depth.attachment);
  for (size_t i = 0; i < base.attachments.size(); ++i) {
    REQUIRE(base.attachments[i].format == current.attachments[i].format);
    REQUIRE(base.attachments[i].samples == current.attachments[i].samples);
    REQUIRE(current.attachments[i].initialLayout ==
            base.attachments[i].initialLayout);
    REQUIRE(current.attachments[i].finalLayout ==
            base.attachments[i].finalLayout);
    REQUIRE(current.attachments[i].storeOp == base.attachments[i].storeOp);
  }
  for (size_t i = 0; i < base.colors.size(); ++i) {
    REQUIRE(base.colors[i].attachment == current.colors[i].attachment);
    REQUIRE(base.colors[i].layout == current.colors[i].layout);
  }
  if (p->clearValueCount) {
    REQUIRE(p->clearValueCount > selectedIndex);
    REQUIRE(current.attachments[selectedIndex].loadOp ==
            VK_ATTACHMENT_LOAD_OP_CLEAR);
    selectedValue = p->pClearValues[selectedIndex].color;
  } else {
    for (auto &a : current.attachments)
      REQUIRE(a.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD);
  }
  REQUIRE(p->renderArea.offset.x == 0 && p->renderArea.offset.y == 0);
  const VkExtent2D extent = framebufferExtents.at(p->framebuffer);
  REQUIRE(p->renderArea.extent.width == extent.width &&
          p->renderArea.extent.height == extent.height);
}
static void endPass(VkCommandBuffer) {
  REQUIRE(inPass);
  inPass = false;
  commands.push_back('E');
}
static void barrier(VkCommandBuffer, VkPipelineStageFlags src,
                    VkPipelineStageFlags dst, VkDependencyFlags flags,
                    uint32_t memories, const VkMemoryBarrier *memory,
                    uint32_t buffers, const VkBufferMemoryBarrier *,
                    uint32_t images, const VkImageMemoryBarrier *image) {
  REQUIRE(!rejectBeforeRecording);
  REQUIRE(!inPass && flags == 0 && buffers == 0);
  if (memories) {
    REQUIRE(memories == 1);
    REQUIRE(memory->srcAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    REQUIRE(memory->dstAccessMask & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    REQUIRE(src & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    REQUIRE(dst & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
  }
  if (!images) {
    REQUIRE(memories == 1);
    commands.push_back('M');
    return;
  }
  REQUIRE(images == 1);
  commands.push_back('I');
  REQUIRE(image->srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED &&
          image->dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
  REQUIRE(image->subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
  REQUIRE(image->subresourceRange.baseMipLevel == 0 &&
          image->subresourceRange.levelCount == 1);
  REQUIRE(image->subresourceRange.baseArrayLayer == 0 &&
          image->subresourceRange.layerCount == 1);
  REQUIRE(imageLayouts.at(image->image) == image->oldLayout);
  if (image->newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    REQUIRE(image->oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    REQUIRE(image->srcAccessMask == (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
    REQUIRE(image->dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
    REQUIRE(src & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    REQUIRE(dst & VK_PIPELINE_STAGE_TRANSFER_BIT);
  } else if (isolation && image->oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
    REQUIRE(image->newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    REQUIRE(image->srcAccessMask == 0);
  } else if (isolation && image->newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    REQUIRE(image->oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    REQUIRE(src & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  } else {
    REQUIRE(image->oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    REQUIRE(image->newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    REQUIRE(image->srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
    REQUIRE(image->dstAccessMask == (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
    REQUIRE(src == VK_PIPELINE_STAGE_TRANSFER_BIT &&
            dst == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    REQUIRE(memories == 0);
  }
  imageLayouts[image->image] = image->newLayout;
}
static void clearImage(VkCommandBuffer, VkImage image, VkImageLayout layout,
                       const VkClearColorValue *color, uint32_t count,
                       const VkImageSubresourceRange *range) {
  REQUIRE(!rejectBeforeRecording);
  REQUIRE(!inPass);
  commands.push_back('K');
  selectedImage = image;
  selectedValue = *color;
  REQUIRE(layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
          imageLayouts.at(image) == layout);
  REQUIRE(count == 1 && range->levelCount == 1 && range->layerCount == 1);
}
static void clearAttachments(VkCommandBuffer, uint32_t attachmentCount,
                             const VkClearAttachment *attachments, uint32_t count,
                             const VkClearRect *rects) {
  REQUIRE(!rejectBeforeRecording);
  REQUIRE(inPass);
  if (depthClearRecording) {
    REQUIRE(attachmentCount == 1 && count > 0);
    recordedDepthAttachment = attachments[0];
    REQUIRE(attachments[0].aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT);
    recordedDepthRects.assign(rects, rects + count);
    for (const auto &r : recordedDepthRects) {
      REQUIRE(r.rect.offset.x >= 0 && r.rect.offset.y >= 0);
      REQUIRE(r.rect.extent.width > 0 && r.rect.extent.height > 0);
      REQUIRE(uint64_t(r.rect.offset.x) + r.rect.extent.width <= expectedFramebufferExtent.width);
      REQUIRE(uint64_t(r.rect.offset.y) + r.rect.extent.height <= expectedFramebufferExtent.height);
      REQUIRE(r.baseArrayLayer == 0 && r.layerCount == 1);
    }
  }
  commands.push_back('C');
}
static void draw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t) {
  REQUIRE(!rejectBeforeRecording && inPass);
  commands.push_back('D');
  if (clearPipelines.count(boundPipeline)) {
    REQUIRE(pushed.size() >= 32);
    RenderRect rectangle;
    std::memcpy(&rectangle, pushed.data() + 16, sizeof(rectangle));
    REQUIRE(rectangle.left == boundScissors[0].offset.x &&
            rectangle.top == boundScissors[0].offset.y);
    REQUIRE(uint32_t(rectangle.right - rectangle.left) ==
            boundScissors[0].extent.width);
    REQUIRE(uint32_t(rectangle.bottom - rectangle.top) ==
            boundScissors[0].extent.height);
    ++clearDraws;
    clearDrawScissors.push_back(boundScissors.at(0));
  }
}
static VkResult resetCommands(VkCommandBuffer, VkCommandBufferResetFlags) {
  REQUIRE(!inPass);
  return VK_SUCCESS;
}
static VkResult beginCommands(VkCommandBuffer,
                              const VkCommandBufferBeginInfo *) {
  return VK_SUCCESS;
}
static VkResult endCommands(VkCommandBuffer) {
  REQUIRE(!inPass);
  return VK_SUCCESS;
}
static VkResult createLayout(VkDevice, const VkPipelineLayoutCreateInfo *p,
                             const VkAllocationCallbacks *,
                             VkPipelineLayout *out) {
  REQUIRE(p->setLayoutCount == 0 && p->pushConstantRangeCount == 1);
  *out = handle<VkPipelineLayout>();
  return VK_SUCCESS;
}
static void destroyLayout(VkDevice, VkPipelineLayout,
                          const VkAllocationCallbacks *) {}
static VkResult createShader(VkDevice, const VkShaderModuleCreateInfo *p,
                             const VkAllocationCallbacks *,
                             VkShaderModule *out) {
  REQUIRE(p->pCode[0] == 0x07230203);
  if (p->codeSize == sizeof(ColorClearFragment))
    REQUIRE(p->pCode[ColorClearFragmentLocationWord] == selectedIndex);
  *out = handle<VkShaderModule>();
  return VK_SUCCESS;
}
static void destroyShader(VkDevice, VkShaderModule,
                          const VkAllocationCallbacks *) {}
static VkResult createPipeline(VkDevice, VkPipelineCache, uint32_t count,
                               const VkGraphicsPipelineCreateInfo *p,
                               const VkAllocationCallbacks *, VkPipeline *out) {
  REQUIRE(!rejectBeforeRecording && count == 1 && p->subpass == 0);
  if (presentationPipelines) {
    REQUIRE(p->pColorBlendState->attachmentCount == 1 &&
            p->pRasterizationState->depthClampEnable == VK_FALSE);
    const auto &attachment = passes.at(p->renderPass).attachments[0];
    REQUIRE(attachment.samples == p->pMultisampleState->rasterizationSamples &&
            attachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD);
    presentationRecords.push_back({attachment.format, attachment.samples,
                                   p->pColorBlendState->pAttachments[0]});
    *out = handle<VkPipeline>();
    return VK_SUCCESS;
  }
  REQUIRE(passes.at(p->renderPass).attachments[selectedIndex].loadOp ==
          VK_ATTACHMENT_LOAD_OP_LOAD);
  REQUIRE(p->pVertexInputState->vertexBindingDescriptionCount == 0);
  REQUIRE(p->pViewportState->viewportCount == 1);
  REQUIRE(!p->pRasterizationState->depthBiasEnable &&
          p->pRasterizationState->cullMode == VK_CULL_MODE_NONE);
  REQUIRE(p->pMultisampleState->pSampleMask == nullptr &&
          !p->pMultisampleState->alphaToCoverageEnable);
  REQUIRE(!p->pDepthStencilState->depthTestEnable &&
          !p->pDepthStencilState->depthWriteEnable &&
          !p->pDepthStencilState->stencilTestEnable);
  for (unsigned i = 0; i < p->pColorBlendState->attachmentCount; ++i) {
    REQUIRE(!p->pColorBlendState->pAttachments[i].blendEnable);
    REQUIRE(p->pColorBlendState->pAttachments[i].colorWriteMask ==
            (i == selectedIndex ? 15 : 0));
  }
  ++drawPipelineCreates;
  *out = handle<VkPipeline>();
  clearPipelines.insert(*out);
  return VK_SUCCESS;
}
static void destroyPipeline(VkDevice, VkPipeline,
                            const VkAllocationCallbacks *) {}
static void bindPipeline(VkCommandBuffer, VkPipelineBindPoint point,
                         VkPipeline pipeline) {
  REQUIRE(point == VK_PIPELINE_BIND_POINT_GRAPHICS);
  boundPipeline = pipeline;
}
static void setViewport(VkCommandBuffer, uint32_t first, uint32_t count,
                        const VkViewport *values) {
  REQUIRE(first == 0);
  boundViewports.assign(values, values + count);
}
static void setScissor(VkCommandBuffer, uint32_t first, uint32_t count,
                       const VkRect2D *values) {
  REQUIRE(first == 0);
  boundScissors.assign(values, values + count);
}
static void setBias(VkCommandBuffer, float bias, float clamp, float slope) {
  boundDepthBias[0] = bias;
  boundDepthBias[1] = clamp;
  boundDepthBias[2] = slope;
}
static void pushConstants(VkCommandBuffer, VkPipelineLayout,
                          VkShaderStageFlags stages, uint32_t offset,
                          uint32_t size, const void *data) {
  REQUIRE(stages == VK_SHADER_STAGE_FRAGMENT_BIT);
  if (pushed.size() < offset + size)
    pushed.resize(offset + size);
  std::memcpy(pushed.data() + offset, data, size);
}
static VkResult createSem(VkDevice, const VkSemaphoreCreateInfo *,
                          const VkAllocationCallbacks *, VkSemaphore *out) {
  *out = handle<VkSemaphore>();
  return VK_SUCCESS;
}
static void destroySem(VkDevice, VkSemaphore, const VkAllocationCallbacks *) {}
static VkResult submit(VkQueue, uint32_t count, const VkSubmitInfo *p,
                       VkFence) {
  REQUIRE(!rejectBeforeRecording);
  REQUIRE(count == 1);
  if (isolation) {
    REQUIRE(isolationSubmits < 5);
    REQUIRE(p->waitSemaphoreCount == (isolationSubmits == 1 ? 1 : 0));
    if (isolationSubmits == 1)
      REQUIRE(p->pWaitSemaphores[0] == acquired && acquireCalls == 1);
    if (isolationSubmits <= 1)
      REQUIRE(commands.empty());
    if (isolationSubmits == 2 || isolationSubmits == 4)
      REQUIRE(commands == std::vector<char>{'I'});
    if (isolationSubmits == 3) {
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                               \
    !TOOIE_VULKAN_TRANSFER_CLEAR_TEST
#if defined(TOOIE_VULKAN_CLEAR_BARRIER_TEST) && TOOIE_VULKAN_CLEAR_BARRIER_TEST
      REQUIRE(commands == std::vector<char>{'M'});
#else
      REQUIRE(commands == std::vector<char>({'M', 'B', 'E'}));
#endif
#else
      REQUIRE(commands == std::vector<char>({'I', 'K', 'I'}));
#endif
    }
    commands.clear();
    ++isolationSubmits;
  } else
    REQUIRE(p->waitSemaphoreCount == 2);
  for (unsigned i = 0; i < p->waitSemaphoreCount; ++i)
    REQUIRE(p->pWaitDstStageMask[i] == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  return VK_SUCCESS;
}
static VkResult createFence(VkDevice, const VkFenceCreateInfo *,
                            const VkAllocationCallbacks *, VkFence *out) {
  *out = handle<VkFence>();
  return VK_SUCCESS;
}
static void destroyFence(VkDevice, VkFence, const VkAllocationCallbacks *) {}
static VkResult waitFence(VkDevice, uint32_t, const VkFence *, VkBool32,
                          uint64_t timeout) {
  REQUIRE(timeout == 5000000000ULL);
  if (isolation)
    ++isolationWaits;
  return simulateTimeout ? VK_TIMEOUT : VK_SUCCESS;
}
static VkResult resetFence(VkDevice, uint32_t, const VkFence *) {
  return VK_SUCCESS;
}
static void initialize(VulkanTexture &texture, VulkanDevice &device,
                       uint32_t samples, RenderFormat format) {
  texture.device = &device;
  texture.vk = handle<VkImage>();
  texture.imageView = handle<VkImageView>();
  texture.desc = RenderTextureDesc::ColorTarget(640, 480, format);
  texture.desc.multisampling.sampleCount = samples;
  texture.imageFormat = format == RenderFormat::B8G8R8A8_UNORM
                            ? VK_FORMAT_B8G8R8A8_UNORM
                            : VK_FORMAT_R8G8B8A8_UNORM;
  texture.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  texture.textureLayout = RenderTextureLayout::COLOR_WRITE;
  texture.barrierStages = RenderBarrierStage::GRAPHICS;
  imageLayouts[texture.vk] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}
// Exercise the real backend method and inspect the exact Vulkan command.
// No GPU or alleged MD-001 draw inputs are substituted into these fixtures.
static void testDepthClearDomain(VulkanDevice &device, VulkanCommandList &list) {
  depthClearRecording = true;
  for (bool mipView : {false, true}) {
    expectedFramebufferExtent = mipView ? VkExtent2D{320, 241} : VkExtent2D{640, 480};
    VulkanTexture depth;
    initialize(depth, device, mipView ? 1 : 2, RenderFormat::R8G8B8A8_UNORM);
    depth.desc.format = RenderFormat::D32_FLOAT;
    depth.desc.flags = RenderTextureFlag::DEPTH_TARGET;
    depth.desc.height = mipView ? 482 : 480;
    depth.desc.mipLevels = mipView ? 2 : 1;
    depth.imageFormat = VK_FORMAT_D32_SFLOAT;
    depth.imageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth.textureLayout = RenderTextureLayout::DEPTH_WRITE;
    imageLayouts[depth.vk] = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    std::unique_ptr<RenderTextureView> view;
    RenderFramebufferDesc desc(nullptr, 0, &depth);
    if (mipView) {
      auto vd = RenderTextureViewDesc::Texture2D(RenderFormat::D32_FLOAT);
      vd.mipSlice = 1;
      vd.mipLevels = vd.arraySize = 1;
      view = depth.createTextureView(vd);
      desc.depthAttachment = nullptr;
      desc.depthAttachmentView = view.get();
    }
    VulkanFramebuffer framebuffer(&device, desc);
    const int32_t w = int32_t(expectedFramebufferExtent.width);
    const int32_t h = int32_t(expectedFramebufferExtent.height);
    auto clear = [&](const RenderRect *rects, uint32_t count) {
      list.begin();
      list.setFramebuffer(&framebuffer);
      commands.clear();
      recordedDepthRects.clear();
      list.clearDepthStencil(true, false, 0.625f, 0, rects, count);
    };
    const RenderRect edge(w - 20, h - 15, w + 40, h + 80);
    clear(&edge, 1);
    REQUIRE(commands == std::vector<char>({'B', 'C'}));
    REQUIRE(recordedDepthRects.size() == 1);
    REQUIRE(recordedDepthRects[0].rect.offset.x == w - 20 && recordedDepthRects[0].rect.offset.y == h - 15);
    REQUIRE(recordedDepthRects[0].rect.extent.width == 20 && recordedDepthRects[0].rect.extent.height == 15);
    REQUIRE(recordedDepthAttachment.clearValue.depthStencil.depth == 0.625f);
    list.setFramebuffer(nullptr); list.end();

    const RenderRect mixed[] = {{-20, -10, 30, 40}, {w + 1, h + 1, w + 2, h + 2},
                                {10, 20, 15, 29}};
    clear(mixed, 3);
    REQUIRE(recordedDepthRects.size() == 2);
    REQUIRE(recordedDepthRects[0].rect.offset.x == 0 && recordedDepthRects[0].rect.offset.y == 0);
    REQUIRE(recordedDepthRects[0].rect.extent.width == 30 && recordedDepthRects[0].rect.extent.height == 40);
    REQUIRE(recordedDepthRects[1].rect.offset.x == 10 && recordedDepthRects[1].rect.offset.y == 20);
    REQUIRE(recordedDepthRects[1].rect.extent.width == 5 && recordedDepthRects[1].rect.extent.height == 9);
    list.setFramebuffer(nullptr); list.end();

    const RenderRect outside[] = {{-30, -20, -1, -1}, {w, h, w + 10, h + 10},
                                  {5, 5, 5, 10}, {8, 8, 2, 2}};
    clear(outside, 4);
    REQUIRE(commands.empty() && recordedDepthRects.empty());
    list.setFramebuffer(nullptr); list.end();

    const RenderRect extreme(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX);
    clear(&extreme, 1);
    REQUIRE(recordedDepthRects.size() == 1);
    REQUIRE(recordedDepthRects[0].rect.offset.x == 0 && recordedDepthRects[0].rect.offset.y == 0);
    REQUIRE(recordedDepthRects[0].rect.extent.width == uint32_t(w) && recordedDepthRects[0].rect.extent.height == uint32_t(h));
    list.setFramebuffer(nullptr); list.end();
    clear(nullptr, 0);
    REQUIRE(recordedDepthRects.size() == 1 && recordedDepthRects[0].rect.extent.width == uint32_t(w) && recordedDepthRects[0].rect.extent.height == uint32_t(h));
    list.setFramebuffer(nullptr); list.end();
  }
  depthClearRecording = false;
  expectedFramebufferExtent = {640, 480};
  std::puts("PASS: production depth clears clip edge/negative/extreme rectangles to the actual framebuffer/mip view; outside batches record no commands; full and in-bounds clears preserved, MSAA2X and 1X.");
}

static void resetSurfaceEnumeration() {
  surfaceCreateResult = supportQueryResult = surfaceQueryResult =
      formatCountResult = formatDataResult = modeCountResult = modeDataResult =
          VK_SUCCESS;
  presentationFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                         VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
  swapImageConfigSupported = true;
  expectedSwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
  queuePresentSupport = VK_TRUE;
  surfaceAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  reportedSurfaceFormats = {
      {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
  formatsGrowOnce = formatsAlwaysIncomplete = formatsShrink =
      formatsOverreported = formatsZeroWritten = false;
  formatCountQueries = formatDataQueries = 0;
}
static void testSurfaceFormatInitialization(VulkanCommandQueue &queue) {
  RenderSwapChainDesc desc;
  desc.renderWindow = reinterpret_cast<ANativeWindow *>(1);
  desc.textureCount = 2;
  desc.format = RenderFormat::B8G8R8A8_UNORM;
  struct Failure {
    const char *reason;
    unsigned mode;
  };
  for (const auto &failure :
       std::vector<Failure>{{"surface-creation-failed", 0},
                            {"present-support-query-failed", 1},
                            {"queue-present-unsupported", 2},
                            {"surface-capabilities-query-failed", 3},
                            {"composite-alpha-unsupported", 4},
                            {"surface-format-count-query-failed", 5},
                            {"surface-format-count-zero", 6},
                            {"surface-format-data-query-failed", 7},
                            {"surface-format-data-count-invalid", 8},
                            {"surface-format-data-count-invalid", 9},
                            {"surface-format-enumeration-incomplete", 10},
                            {"requested-surface-format-unsupported", 11},
                            {"undefined-surface-format-vulkan-1.2", 12},
                            {"present-mode-count-query-failed", 13},
                            {"present-mode-data-query-failed", 14},
                            {"undefined-surface-format-vulkan-1.2", 15},
                            {"surface-format-features-unsupported", 16},
                            {"requested-surface-format-unsupported", 17},
                            {"surface-format-features-unsupported", 18}}) {
    resetSurfaceEnumeration();
    switch (failure.mode) {
    case 0:
      surfaceCreateResult = VK_ERROR_SURFACE_LOST_KHR;
      break;
    case 1:
      supportQueryResult = VK_ERROR_DEVICE_LOST;
      break;
    case 2:
      queuePresentSupport = VK_FALSE;
      break;
    case 3:
      surfaceQueryResult = VK_ERROR_SURFACE_LOST_KHR;
      break;
    case 4:
      surfaceAlpha = 0;
      break;
    case 5:
      formatCountResult = VK_ERROR_SURFACE_LOST_KHR;
      break;
    case 6:
      reportedSurfaceFormats.clear();
      break;
    case 7:
      formatDataResult = VK_ERROR_SURFACE_LOST_KHR;
      break;
    case 8:
      formatsOverreported = true;
      break;
    case 9:
      formatsZeroWritten = true;
      break;
    case 10:
      formatsAlwaysIncomplete = true;
      break;
    case 11:
      reportedSurfaceFormats = {
          {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
      break;
    case 12:
      reportedSurfaceFormats = {
          {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
      break;
    case 13:
      modeCountResult = VK_ERROR_SURFACE_LOST_KHR;
      break;
    case 14:
      modeDataResult = VK_INCOMPLETE;
      break;
    case 15:
      reportedSurfaceFormats.push_back(
          {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
      break;
    case 16:
      presentationFeatures &= ~VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
      break;
    case 17:
      reportedSurfaceFormats = {
          {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT}};
      break;
    case 18:
      reportedSurfaceFormats = {
          {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
      presentationFeatures &= ~VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
      break;
    }
    const unsigned creates = swapchainCreates;
    VulkanSwapChain rejected(&queue, desc);
    REQUIRE(!rejected.initialized &&
            !std::strcmp(rejected.initializationFailure, failure.reason));
    REQUIRE(rejected.pickedSurfaceFormat.format == VK_FORMAT_UNDEFINED &&
            rejected.isEmpty());
    REQUIRE(queue.swapChains.count(&rejected) == 0 &&
            rejected.getFormat() == RenderFormat::UNKNOWN);
    // Reproduce the old constructor-return -> needsResize -> resize path. It
    // must now stop before even querying capabilities, regardless of the
    // reason.
    const unsigned queried = capabilitiesQueries;
    REQUIRE(rejected.needsResize() && !rejected.resize());
    REQUIRE(!rejected.acquireTexture(nullptr, nullptr));
    REQUIRE(!rejected.present(0, nullptr, 0));
    REQUIRE(rejected.getRefreshRate() == 0);
    rejected.wait();
    REQUIRE(swapchainCreates == creates && capabilitiesQueries == queried &&
            rejected.textures.empty());
    if (failure.mode == 5 || failure.mode == 6)
      REQUIRE(formatDataQueries == 0);
    if (failure.mode == 10)
      REQUIRE(formatCountQueries == 4 && formatDataQueries == 4);
  }
  for (unsigned mode = 0; mode < 6; ++mode) {
    resetSurfaceEnumeration();
    if (mode == 0)
      reportedSurfaceFormats = {
          {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT},
          {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
          {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    if (mode == 1) {
      reportedSurfaceFormats.push_back(
          {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
      formatsGrowOnce = true;
    }
    if (mode == 2) {
      reportedSurfaceFormats.push_back(
          {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
      formatsShrink = true;
    }
    if (mode == 3)
      reportedSurfaceFormats[0].colorSpace =
          VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
    if (mode == 4) {
      // Exact format list from the d0e64bf Odin test (BGRA8 is absent).
      reportedSurfaceFormats = {
          {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
          {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
          {VK_FORMAT_R5G6B5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
          {VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
          {VK_FORMAT_A2B10G10R10_UNORM_PACK32,
           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
          {VK_FORMAT_R8_UNORM, VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT}};
      expectedSwapchainFormat = VK_FORMAT_R8G8B8A8_UNORM;
    }
    if (mode == 5)
      reportedSurfaceFormats.insert(
          reportedSurfaceFormats.begin(),
          {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    const RenderFormat expectedRhi =
        mode == 4 ? RenderFormat::R8G8B8A8_UNORM : desc.format;
    const unsigned creates = swapchainCreates;
    VulkanSwapChain accepted(&queue, desc);
    REQUIRE(accepted.initialized &&
            !std::strcmp(accepted.initializationFailure, "none"));
    REQUIRE(accepted.pickedSurfaceFormat.format == expectedSwapchainFormat);
    REQUIRE(accepted.pickedSurfaceFormat.colorSpace ==
            (mode == 3 ? VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT
                       : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR));
    REQUIRE(accepted.desc.format == expectedRhi &&
            accepted.getFormat() == expectedRhi && accepted.resize() &&
            swapchainCreates == creates + 1);
    REQUIRE(accepted.createInfo.imageFormat == expectedSwapchainFormat);
    if (mode == 1)
      REQUIRE(formatCountQueries == 2 && formatDataQueries == 2);
    for (auto &texture : accepted.textures) {
      REQUIRE(texture.imageFormat == accepted.createInfo.imageFormat &&
              texture.desc.format == expectedRhi);
      REQUIRE(actualViewFormats.at(texture.imageView) == texture.imageFormat);
      const RenderTexture *color = &texture;
      VulkanFramebuffer fb(queue.device, RenderFramebufferDesc(&color, 1));
      REQUIRE(fb.getColorAttachmentFormat(0) == expectedRhi &&
              fb.getColorAttachmentFormat(1) == RenderFormat::UNKNOWN);
      REQUIRE(fb.colorAttachmentFormats[0] == texture.imageFormat &&
              fb.colorAttachmentImageViews[0] == texture.imageView);
      REQUIRE(passes.at(fb.renderPass).attachments[0].format ==
              texture.imageFormat);
    }
    // Defence in depth: even a malformed initialized object cannot pass format
    // zero or stale RHI/image format metadata into vkCreateSwapchainKHR.
    const unsigned queried = capabilitiesQueries;
    const auto oldView = accepted.textures[0].imageView;
    accepted.pickedSurfaceFormat.format = VK_FORMAT_UNDEFINED;
    REQUIRE(!accepted.resize());
    accepted.pickedSurfaceFormat.format =
        mode == 4 ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
    REQUIRE(!accepted.resize() && swapchainCreates == creates + 1 &&
            capabilitiesQueries == queried);
    REQUIRE(accepted.textures[0].imageView == oldView);
    // Complete image usage/tiling/sample-count support is a separate query
    // from the surface format list. A rejected replacement keeps old views.
    accepted.pickedSurfaceFormat.format = expectedSwapchainFormat;
    swapImageConfigSupported = false;
    const unsigned imageQueriesBefore = swapImageQueries;
    REQUIRE(!accepted.resize() && swapImageQueries == imageQueriesBefore + 1);
    REQUIRE(swapchainCreates == creates + 1 &&
            accepted.textures[0].imageView == oldView);
  }
  resetSurfaceEnumeration();
  std::puts(
      "PASS: failed/incomplete surface enumeration, constructor early returns, "
      "invalid-object guards, and image/view/framebuffer format consistency");
}
static void testPresentationPipelineFormats(VulkanDevice &device) {
  selectedIndex = 0;
  presentationPipelines = true;
  const auto count = presentationRecords.size();
  VulkanShader vertex(&device, ColorClearVertex, sizeof(ColorClearVertex),
                      "main", RenderShaderFormat::SPIRV);
  VulkanShader pixel(&device, ColorClearFragment, sizeof(ColorClearFragment),
                     "main", RenderShaderFormat::SPIRV);
  RenderPushConstantRange range(0, 0, 0, 32, RenderShaderStageFlag::PIXEL);
  VulkanPipelineLayout layout(&device,
                              RenderPipelineLayoutDesc(&range, 1, nullptr, 0));
  RenderGraphicsPipelineDesc base;
  base.vertexShader = &vertex;
  base.pixelShader = &pixel;
  base.pipelineLayout = &layout;
  recompui::UiPresentationPipelines cache;
  for (auto format :
       {RenderFormat::R8G8B8A8_UNORM, RenderFormat::B8G8R8A8_UNORM}) {
    VulkanTexture texture;
    initialize(texture, device, 1, format);
    const RenderTexture *color = &texture;
    VulkanFramebuffer fb(&device, RenderFramebufferDesc(&color, 1));
    for (unsigned variant = 0; variant < 3; ++variant) {
      auto vi = RT64::CreateVideoInterfacePipeline(
          &device, fb.getColorAttachmentFormat(0), &vertex, &pixel, &layout);
      const auto &record = presentationRecords.back();
      REQUIRE(record.format == texture.imageFormat &&
              record.samples == VK_SAMPLE_COUNT_1_BIT &&
              !record.blend.blendEnable);
    }
    for (bool composite : {false, true}) {
      auto *pipeline = cache.get(&device, base, &fb, composite);
      const auto &record = presentationRecords.back();
      REQUIRE(record.format == texture.imageFormat &&
              record.samples == VK_SAMPLE_COUNT_1_BIT &&
              record.blend.blendEnable);
      REQUIRE(record.blend.srcColorBlendFactor ==
              (composite ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA));
      REQUIRE(record.blend.dstColorBlendFactor ==
                  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
              record.blend.srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
              record.blend.dstAlphaBlendFactor ==
                  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
              record.blend.colorWriteMask == 15);
      const auto records = presentationRecords.size();
      REQUIRE(cache.get(&device, base, &fb, composite) == pipeline &&
              presentationRecords.size() == records);
    }
  }
  auto offscreen = recompui::CreateUiPipeline(
      &device, base, recompui::UiOffscreenFormat, RenderMultisampling(8),
      recompui::UiPipelinePath::Offscreen);
  REQUIRE(presentationRecords.back().format == VK_FORMAT_B8G8R8A8_UNORM &&
          presentationRecords.back().samples == VK_SAMPLE_COUNT_8_BIT &&
          presentationRecords.back().blend.srcColorBlendFactor ==
              VK_BLEND_FACTOR_SRC_ALPHA);
  REQUIRE(presentationRecords.size() == count + 11);
  presentationPipelines = false;
  std::puts("PASS: VI nearest/linear/pixel and UI direct/offscreen/composite "
            "pipeline formats and alpha factors");
}
#if !defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                              \
    !defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK)
static void testAttachmentClearDomain(VulkanDevice &device,
                                      VulkanCommandList &list) {
  selectedIndex = 0;
  expectedFramebufferExtent = {320, 241};
  for (bool mipView : {false, true}) {
    list.begin();
    VulkanTexture texture;
    initialize(texture, device, mipView ? 1 : 2, RenderFormat::R8G8B8A8_UNORM);
    texture.desc.width = mipView ? 640 : 320;
    texture.desc.height = mipView ? 482 : 241;
    texture.desc.mipLevels = mipView ? 2 : 1;
    std::unique_ptr<RenderTextureView> view;
    if (mipView) {
      auto desc = RenderTextureViewDesc::Texture2D(texture.desc.format);
      desc.mipSlice = 1;
      desc.mipLevels = desc.arraySize = 1;
      view = texture.createTextureView(desc);
    }
    const RenderTexture *color = &texture;
    const RenderTextureView *attachmentView = view.get();
    RenderFramebufferDesc desc(&color, 1);
    desc.colorAttachmentViews = &attachmentView;
    VulkanFramebuffer framebuffer(&device, desc);
    REQUIRE(framebuffer.getWidth() == 320 && framebuffer.getHeight() == 241);
    list.setFramebuffer(&framebuffer);
    commands.clear();
    clearDrawScissors.clear();
    const RenderRect original = {310, 230, 331, 263};
    list.clearColor(0, RenderColor(), &original, 1);
    REQUIRE(commands == std::vector<char>({'M', 'B', 'D'}) &&
            clearDrawScissors.size() == 1);
    const VkRect2D &scissor = clearDrawScissors[0];
    REQUIRE(scissor.offset.x == 310 && scissor.offset.y == 230 &&
            scissor.extent.width == 10 && scissor.extent.height == 11);
    RenderRect pushedRectangle;
    std::memcpy(&pushedRectangle, pushed.data() + 16, sizeof(pushedRectangle));
    REQUIRE(pushedRectangle == RenderRect(310, 230, 320, 241));
    REQUIRE(passes.at(framebuffer.renderPass).attachments[0].samples ==
            (mipView ? VK_SAMPLE_COUNT_1_BIT : VK_SAMPLE_COUNT_2_BIT));
    list.setFramebuffer(nullptr);
    list.end();
    std::printf("PASS: clipping domain fbExtent=320x241 imageExtent=%ux%u "
                "viewMip=%u samples=%u\n",
                texture.desc.width, texture.desc.height, unsigned(mipView),
                texture.desc.multisampling.sampleCount);
  }
  expectedFramebufferExtent = {640, 480};
}
#endif
static void testAndroidSurfaceLifecycle(VulkanCommandQueue &queue, VulkanCommandList &list) {
  using Lifecycle = tooie::android::PresentationLifecycle;
  for (bool rgba : {false, true}) {
    resetSurfaceEnumeration();
    if (rgba) {
      reportedSurfaceFormats = {{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
      expectedSwapchainFormat = VK_FORMAT_R8G8B8A8_UNORM;
    }
    expectedImageInfo = {};
    expectedImageInfo.imageType = VK_IMAGE_TYPE_2D;
    expectedImageInfo.format = expectedSwapchainFormat;
    expectedImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    expectedImageInfo.usage = surfaceUsage;
    expectedImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageLimits.maxExtent = {4096, 4096, 1};
    imageLimits.maxMipLevels = imageLimits.maxArrayLayers = 1;
    imageLimits.sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    RenderSwapChainDesc desc;
    desc.format = RenderFormat::B8G8R8A8_UNORM;
    desc.textureCount = 2;
    desc.renderWindow = reinterpret_cast<ANativeWindow *>(uintptr_t(100));
    VulkanSwapChain chain(&queue, desc);
    REQUIRE(chain.initialized && chain.resize());
    const RenderFormat format = chain.getFormat();
    Lifecycle lifecycle;
    std::vector<std::unique_ptr<RenderFramebuffer>> frames;
    std::vector<std::unique_ptr<RenderCommandSemaphore>> draws;
    std::unique_ptr<RenderCommandSemaphore> acquire;
    lifecycleRecording = true;
    { auto lock = lifecycle.lock(); lifecycle.attach(&chain, [&] {
        RT64::ResetAndroidPresentation(&chain, &list, frames, draws, acquire);
        chain.suspendSurface();
      }); }
    for (unsigned cycle = 0; cycle < 24; ++cycle) {
      { auto lock = lifecycle.lock();
        REQUIRE(lifecycle.available());
        if (chain.isSurfaceSuspended()) {
          REQUIRE(chain.resumeSurface(reinterpret_cast<ANativeWindow *>(uintptr_t(101 + cycle))));
          REQUIRE(chain.resize());
        }
        REQUIRE(chain.getFormat() == format && chain.createInfo.imageFormat == expectedSwapchainFormat);
        for (auto &texture : chain.textures) {
          const RenderTexture *color = &texture;
          frames.emplace_back(std::make_unique<VulkanFramebuffer>(queue.device, RenderFramebufferDesc(&color, 1)));
          REQUIRE(frames.back()->getColorAttachmentFormat(0) == format);
          draws.emplace_back(queue.device->createCommandSemaphore());
        }
        acquire = queue.device->createCommandSemaphore();
        uint32_t index = 0;
        REQUIRE(chain.acquireTexture(acquire.get(), &index));
        REQUIRE(chain.present(index, nullptr, 0));
        lifecycle.markLive();
      }
      lifecycleOrder.clear();
      lifecycle.update(cycle % 2 ? Lifecycle::Event::FocusLost : Lifecycle::Event::Pause);
      REQUIRE(frames.empty() && draws.empty() && !acquire && chain.isEmpty());
      REQUIRE(chain.vk == VK_NULL_HANDLE && chain.surface == VK_NULL_HANDLE && chain.getWindow() == nullptr);
      REQUIRE(lifecycleOrder == std::vector<char>({'Q','F','F','V','V','W','S'}));
      // Repeated pause/stop/destroy-surface notifications must be idempotent.
      lifecycle.update(Lifecycle::Event::SurfaceUnavailable);
      lifecycle.update(Lifecycle::Event::Pause);
      REQUIRE(lifecycleOrder.size() == 7);
      REQUIRE(!chain.acquireTexture(nullptr, nullptr) && !chain.present(0, nullptr, 0) && !chain.resize());
      lifecycle.update(Lifecycle::Event::Resume);
      { auto lock = lifecycle.lock(); REQUIRE(!lifecycle.available()); }
      lifecycle.update(Lifecycle::Event::SurfaceReady);
      lifecycle.update(Lifecycle::Event::FocusGained);
    }
    // A replacement surface may fail validation. Do not change the format or
    // reuse the old window/views; allow a later valid surface to recover.
    reportedSurfaceFormats.clear();
    REQUIRE(!chain.resumeSurface(reinterpret_cast<ANativeWindow *>(uintptr_t(999))));
    REQUIRE(chain.isSurfaceSuspended() && chain.isEmpty() && !chain.resize());
    reportedSurfaceFormats = {{expectedSwapchainFormat, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    REQUIRE(chain.resumeSurface(reinterpret_cast<ANativeWindow *>(uintptr_t(1000))));
    REQUIRE(chain.resize() && chain.getFormat() == format);
    chain.waitForIdle();
    swapchainCreateResult = VK_ERROR_OUT_OF_DATE_KHR;
    REQUIRE(!chain.resize() && chain.isEmpty());
    swapchainCreateResult = VK_SUCCESS;
    REQUIRE(chain.resize());
    viewCreateFails = true;
    REQUIRE(!chain.resize() && chain.isEmpty());
    viewCreateFails = false;
    REQUIRE(chain.resize());
    swapchainImageCountResult = VK_ERROR_SURFACE_LOST_KHR;
    REQUIRE(!chain.resize() && chain.isEmpty());
    swapchainImageCountResult = VK_SUCCESS;
    REQUIRE(chain.resize());
    for (int incompleteCount : {0, 1, 3}) {
      imageDataCount = incompleteCount;
      REQUIRE(!chain.resize() && chain.isEmpty() && chain.getTextureCount() == 2);
      imageDataCount = -1;
      REQUIRE(chain.resize()); // Required creation count remains valid.
    }
    acquire = queue.device->createCommandSemaphore();
    acquireResult = VK_TIMEOUT;
    uint32_t index = 0;
    REQUIRE(!chain.acquireTexture(acquire.get(), &index) && !chain.isSurfaceSuspended());
    acquireResult = VK_ERROR_SURFACE_LOST_KHR;
    REQUIRE(!chain.acquireTexture(acquire.get(), &index) && chain.isSurfaceSuspended() && chain.isEmpty());
    acquireResult = VK_SUCCESS;
    { auto lock = lifecycle.lock(); lifecycle.markLive(); }
    lifecycle.update(Lifecycle::Event::Destroy);
    { auto lock = lifecycle.lock(); lifecycle.detach(&chain); }
    lifecycleRecording = false;
    for (const auto &entry : windowReferences) REQUIRE(entry.second == 0);
  }
  resetSurfaceEnumeration();
  std::puts("PASS: Android WSI lifecycle BGRA/RGBA pause/resume, native-window ownership, queue retirement, framebuffer/view ordering, failed creation and surface-loss recovery");
}

static void testRegularScissorBounds(VulkanCommandList &list) {
  // The captured 304x228 HUD scissor in its expanded 320x241 native target can
  // give left-aligned glyphs a negative scissor after integer alignment. Vulkan
  // forbids negative offsets even when the glyph itself is inside the target.
  const int32_t low = std::numeric_limits<int32_t>::min();
  const int32_t high = std::numeric_limits<int32_t>::max();
  const RenderRect cases[] = {
      {0, 0, 320, 240}, {HudCapture::ClipLeft, HudCapture::ClipTop,
                       HudCapture::ClipRight, HudCapture::ClipBottom}, {-4, -3, 100, 200},
      {-20, 2, -1, 18}, {2, -20, 18, -1}, {-30, -20, -2, -1},
      {20, 10, 10, 30}, {10, 20, 30, 10}, {5, 7, 5, 7},
      {low, low, high, high}, {high, high, low, low},
      {high - 2, high - 3, high, high}, {4, 5, 9, 12}};
  unsigned checked = 0;
  auto check = [&](const RenderRect &source, const VkRect2D &recorded) {
    REQUIRE(recorded.offset.x >= 0 && recorded.offset.y >= 0);
    REQUIRE(int64_t(recorded.offset.x) + recorded.extent.width <= high);
    REQUIRE(int64_t(recorded.offset.y) + recorded.extent.height <= high);
    // Independent pixel-membership oracle: clipping to the legal coordinate
    // domain preserves every covered nonnegative pixel, including edges and
    // the captured glyph region. Inverted/outside rectangles cover nothing.
    std::vector<int64_t> xs = {0, 1, 7, 31, 184, 219, 224, 259, 1518, high};
    std::vector<int64_t> ys = {0, 1, 7, 31, 995, 1064, 1139, high};
    for (int64_t edge : {int64_t(source.left), int64_t(source.right)})
      for (int delta : {-1, 0, 1}) if (edge + delta >= 0 && edge + delta <= high) xs.push_back(edge + delta);
    for (int64_t edge : {int64_t(source.top), int64_t(source.bottom)})
      for (int delta : {-1, 0, 1}) if (edge + delta >= 0 && edge + delta <= high) ys.push_back(edge + delta);
    for (int64_t x : xs) for (int64_t y : ys) {
      const bool before = x >= source.left && x < source.right && y >= source.top && y < source.bottom;
      const bool after = x >= recorded.offset.x && x < int64_t(recorded.offset.x) + recorded.extent.width &&
                         y >= recorded.offset.y && y < int64_t(recorded.offset.y) + recorded.extent.height;
      REQUIRE(before == after);
    }
    ++checked;
  };
  for (const auto &rectangle : cases) {
    list.setScissors(&rectangle, 1);
    REQUIRE(boundScissors.size() == 1);
    check(rectangle, boundScissors[0]);
  }
  list.setScissors(cases, uint32_t(std::size(cases)));
  REQUIRE(boundScissors.size() == std::size(cases));
  for (size_t i = 0; i < std::size(cases); ++i) check(cases[i], boundScissors[i]);
  // Cached graphics state is used by the existing temporary-clear restoration
  // tests below; it must retain the same legal scissors sent to Vulkan.
  REQUIRE(list.graphicsScissors.size() == boundScissors.size());
  for (size_t i = 0; i < boundScissors.size(); ++i)
    REQUIRE(std::memcmp(&list.graphicsScissors[i], &boundScissors[i], sizeof(VkRect2D)) == 0);
  std::printf("PASS: %u regular Vulkan scissor cases preserve nonnegative pixel coverage, captured HUD alignment, arrays, extremes and cached restoration state.\n", checked);
}

int main(int argc, char **argv) {
  simulateTimeout = argc > 1 && std::strcmp(argv[1], "--timeout") == 0;
  invalidState = argc > 1 && std::strcmp(argv[1], "--invalid-state") == 0;
  VulkanInterface interface; // A host loader may reject Android surface
                             // support. No GPU is used.
  vkEnumeratePhysicalDevices = enumerate;
  VulkanDevice device(&interface, "");
  vkGetPhysicalDeviceFormatProperties = formatProperties;
  vkGetPhysicalDeviceImageFormatProperties = imageProperties;
  vkCreateAndroidSurfaceKHR = createSurface;
  vkDestroySurfaceKHR = destroySurface;
  vkGetPhysicalDeviceSurfaceSupportKHR = presentSupport;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR = surfaceCaps;
  vkGetPhysicalDeviceSurfaceFormatsKHR = surfaceFormats;
  vkGetPhysicalDeviceSurfacePresentModesKHR = presentModes;
  vkCreateSwapchainKHR = createSwapchain;
  vkDestroySwapchainKHR = destroySwapchain;
  vkGetSwapchainImagesKHR = swapchainImages;
  vkCreateRenderPass = createPass;
  vkDestroyRenderPass = destroyPass;
  vkCreateFramebuffer = createFb;
  vkDestroyFramebuffer = destroyFb;
  vkCreateImageView = createView;
  vkDestroyImageView = destroyView;
  vkCreateCommandPool = createPool;
  vkDestroyCommandPool = destroyPool;
  vkAllocateCommandBuffers = allocate;
  vkFreeCommandBuffers = freeBuffers;
  vkCmdBeginRenderPass = beginPass;
  vkCmdEndRenderPass = endPass;
  vkCmdPipelineBarrier = barrier;
  vkCmdClearColorImage = clearImage;
  vkCmdClearAttachments = clearAttachments;
  vkCmdDraw = draw;
  vkResetCommandBuffer = resetCommands;
  vkBeginCommandBuffer = beginCommands;
  vkEndCommandBuffer = endCommands;
  vkCreatePipelineLayout = createLayout;
  vkDestroyPipelineLayout = destroyLayout;
  vkCreateShaderModule = createShader;
  vkDestroyShaderModule = destroyShader;
  vkCreateGraphicsPipelines = createPipeline;
  vkDestroyPipeline = destroyPipeline;
  vkCmdBindPipeline = bindPipeline;
  vkCmdSetViewport = setViewport;
  vkCmdSetScissor = setScissor;
  vkCmdSetDepthBias = setBias;
  vkCmdPushConstants = pushConstants;
  vkAcquireNextImageKHR = acquireImage;
  vkQueueWaitIdle = queueIdle;
  vkQueuePresentKHR = queuePresent;
  vkCreateSemaphore = createSem;
  vkDestroySemaphore = destroySem;
  vkQueueSubmit = submit;
  vkCreateFence = createFence;
  vkDestroyFence = destroyFence;
  vkWaitForFences = waitFence;
  vkResetFences = resetFence;
  device.queueFamilies.resize(1);
  device.queueFamilies[0].queues.resize(1);
  device.queueFamilies[0].queues[0].mutex = std::make_unique<std::mutex>();
  VulkanCommandQueue queue(&device, RenderCommandListType::DIRECT);
  VulkanCommandList list(&queue);
  if (argc == 1 || (argc == 2 && !std::strcmp(argv[1], "--hud-scissors-only"))) {
    testRegularScissorBounds(list);
    if (argc == 2) return 0;
  }
  device.independentBlendSupported = true;
  if (argc > 1 && std::strcmp(argv[1], "--depth-rects-only") == 0) {
    testDepthClearDomain(device, list);
    REQUIRE(destroyedPasses == createdPasses && framebufferExtents.empty());
    return 0;
  }
  if (argc > 1 && std::strncmp(argv[1], "--framebuffer-reject=", 21) == 0) {
    VulkanTexture texture;
    initialize(texture, device, 1, RenderFormat::R8G8B8A8_UNORM);
    texture.isSwapChainImage = true;
    const char *reason = argv[1] + 21;
    if (!std::strcmp(reason, "null-image")) texture.vk = VK_NULL_HANDLE;
    if (!std::strcmp(reason, "null-view")) texture.imageView = VK_NULL_HANDLE;
    if (!std::strcmp(reason, "format-mismatch")) texture.desc.format = RenderFormat::B8G8R8A8_UNORM;
    rejectBeforeRecording = true;
    const RenderTexture *color = &texture;
    VulkanFramebuffer fb(&device, RenderFramebufferDesc(&color, 1));
    REQUIRE(false); // Must reject before the driver's vkCreateFramebuffer.
  }
  if (argc > 1 && std::strncmp(argv[1], "--pipeline-reject=", 18) == 0) {
    RenderGraphicsPipelineDesc base;
    presentationFeatures &= ~VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    rejectBeforeRecording = true;
    auto rejected = recompui::CreateUiPipeline(
        &device, base,
        std::strcmp(argv[1] + 18, "bgra") == 0 ? RenderFormat::B8G8R8A8_UNORM
                                               : RenderFormat::R8G8B8A8_UNORM,
        RenderMultisampling(8), recompui::UiPipelinePath::Offscreen);
    REQUIRE(false); // Must stop before render-pass/pipeline creation or submit.
  }
  if (argc > 1 && std::strncmp(argv[1], "--isolation", 11) == 0) {
    isolation = true;
    acquireFails = std::strcmp(argv[1], "--isolation-acquire-fails") == 0;
    simulateTimeout = std::strcmp(argv[1], "--isolation-timeout") == 0;
    RenderSwapChainDesc desc;
    desc.renderWindow = reinterpret_cast<ANativeWindow *>(1);
    desc.format = RenderFormat::B8G8R8A8_UNORM;
    desc.textureCount = 2;
    if (std::strcmp(argv[1], "--isolation-rgba") == 0) {
      reportedSurfaceFormats = {
          {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
      expectedSwapchainFormat = VK_FORMAT_R8G8B8A8_UNORM;
    }
    VulkanSwapChain swapchain(&queue, desc);
    if (std::strcmp(argv[1], "--isolation-surface-fails") == 0)
      surfaceUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VulkanCommandFence fence(&device);
    const bool done = RT64::RunVulkanQueueDiagnostic(&device, &swapchain, &list,
                                                     &queue, &fence);
    REQUIRE(done == (!acquireFails &&
                     (surfaceUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)));
    REQUIRE(isolationSubmits == (done ? 5 : acquireFails ? 1 : 0));
    REQUIRE(isolationWaits == isolationSubmits && clearDraws == 0);
    if (done)
      REQUIRE(imageLayouts.at(
                  static_cast<VulkanTexture *>(swapchain.getTexture(0))->vk) ==
              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    std::puts("PASS: isolated A-E submissions, single acquire wait, per-phase "
              "fence, no raster work");
    return 0;
  }
  const char *drawReject =
      argc > 1 && std::strncmp(argv[1], "--draw-reject=", 14) == 0
          ? argv[1] + 14
          : nullptr;
  if (argc > 1 && std::strncmp(argv[1], "--image-reject=", 15) == 0) {
    const char *mode = argv[1] + 15;
    RenderMultisampling ms;
    ms.sampleCount = 8;
    expectedImageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    expectedImageInfo.imageType = VK_IMAGE_TYPE_2D;
    expectedImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    expectedImageInfo.usage =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageLimits.maxExtent = {4096, 4096, 1};
    imageLimits.maxMipLevels = 12;
    imageLimits.maxArrayLayers = 256;
    imageLimits.sampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_8_BIT;
    if (std::strcmp(mode, "query") == 0)
      imageQueryResult = VK_ERROR_FORMAT_NOT_SUPPORTED;
    else if (std::strcmp(mode, "samples") == 0)
      imageLimits.sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    else if (std::strcmp(mode, "extent") == 0)
      imageLimits.maxExtent.width = 32;
    else if (std::strcmp(mode, "mips") == 0)
      imageLimits.maxMipLevels = 0;
    else if (std::strcmp(mode, "layers") == 0)
      imageLimits.maxArrayLayers = 0;
    VulkanTexture rejected(&device, nullptr,
                           RenderTextureDesc::ColorTarget(
                               640, 480, RenderFormat::B8G8R8A8_UNORM, ms));
    REQUIRE(false); // Constructor must stop before VMA allocation.
  }
  if (argc > 1 && std::strncmp(argv[1], "--reject=", 9) == 0)
    clearRejectionCase = argv[1] + 9;
  if (clearRejectionCase == nullptr && !invalidState) {
    testImageConfigurations(device);
    testSwapchainUsage(queue);
    testAndroidSurfaceLifecycle(queue, list);
    testSurfaceFormatInitialization(queue);
    testPresentationPipelineFormats(device);
  }
  if (argc > 1 && std::strcmp(argv[1], "--late-categories") == 0) {
    VulkanTexture warmup;
    initialize(warmup, device, 1, RenderFormat::B8G8R8A8_UNORM);
    warmup.ownership = true;
    const RenderTexture *color = &warmup;
    VulkanFramebuffer framebuffer(&device, RenderFramebufferDesc(&color, 1));
    list.setFramebuffer(&framebuffer);
    for (unsigned i = 0; i < 32; ++i)
      list.clearColor(0, RenderColor(), nullptr, 0);
    list.setFramebuffer(nullptr);
    warmup.ownership = false;
  }
  for (uint32_t samples : {1u, 2u, 8u})
    for (unsigned count : {1u, 2u}) {
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                               \
    TOOIE_VULKAN_TRANSFER_CLEAR_TEST &&                                        \
    !defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK)
      if (samples > 1)
        continue;
#endif
      VulkanTexture first, second, depth;
      RenderFormat format = samples == 8 ? RenderFormat::B8G8R8A8_UNORM
                                         : RenderFormat::R8G8B8A8_UNORM;
      initialize(first, device, samples, format);
      initialize(second, device, samples, format);
      depth.desc =
          RenderTextureDesc::DepthTarget(640, 480, RenderFormat::D32_FLOAT);
      depth.desc.multisampling.sampleCount = samples;
      depth.textureLayout = RenderTextureLayout::DEPTH_WRITE;
      depth.barrierStages = RenderBarrierStage::GRAPHICS;
      depth.device = &device;
      depth.vk = handle<VkImage>();
      depth.imageView = handle<VkImageView>();
      const RenderTexture *colors[] = {&first, &second};
      selectedIndex = count - 1;
      auto &selected = count == 1 ? first : second;
      // Synthetic offscreen ownership metadata; never hand mock images to VMA.
      selected.ownership = samples > 1;
      selected.isSwapChainImage = samples == 1;
      unsigned base = createdPasses;
      {
        VulkanFramebuffer framebuffer(
            &device, RenderFramebufferDesc(colors, count,
                                           count == 2 ? &depth : nullptr));
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                               \
    !TOOIE_VULKAN_TRANSFER_CLEAR_TEST
        REQUIRE(createdPasses == base + 1 + count);
        for (unsigned i = 0; i < count; ++i) {
          const auto &p = passes.at(framebuffer.colorClearRenderPasses[i]);
          for (unsigned j = 0; j < count; ++j)
            REQUIRE(p.attachments[j].loadOp ==
                    (i == j ? VK_ATTACHMENT_LOAD_OP_CLEAR
                            : VK_ATTACHMENT_LOAD_OP_LOAD));
          if (count == 2)
            REQUIRE(p.attachments.back().loadOp == VK_ATTACHMENT_LOAD_OP_LOAD);
        }
#else
        REQUIRE(createdPasses == base + 1 &&
                framebuffer.colorClearRenderPasses.empty());
#endif
        REQUIRE(framebuffer.colorAttachmentImageViews[selectedIndex] ==
                selected.imageView);
        list.setFramebuffer(&framebuffer);
        commands.clear();
        if (invalidState) {
          selected.textureLayout = RenderTextureLayout::PRESENT;
          rejectBeforeRecording = true;
        }
        if (clearRejectionCase != nullptr) {
          const char *r = clearRejectionCase;
          if (!std::strcmp(r, "explicit-attachment-view"))
            framebuffer.colorAttachmentUsesBaseView[selectedIndex] = false;
          if (!std::strcmp(r, "not-2d"))
            selected.desc.dimension = RenderTextureDimension::TEXTURE_3D;
          if (!std::strcmp(r, "linear-tiling"))
            selected.desc.textureArrangement =
                RenderTextureArrangement::ROW_MAJOR;
          if (!std::strcmp(r, "multiple-mip-levels"))
            selected.desc.mipLevels = 2;
          if (!std::strcmp(r, "multiple-array-layers"))
            selected.desc.arraySize = 2;
          if (!std::strcmp(r, "framebuffer-extent-mismatch"))
            selected.desc.width = 1280;
          if (!std::strcmp(r, "missing-transfer-dst-usage"))
            selected.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
          if (!std::strcmp(r, "attachment-format-mismatch"))
            framebuffer.colorAttachmentFormats[selectedIndex] =
                VK_FORMAT_R8G8B8A8_SRGB;
          if (!std::strcmp(r, "unsupported-clear-format")) {
            selected.imageFormat = VK_FORMAT_R8G8B8A8_UINT;
            framebuffer.colorAttachmentFormats[selectedIndex] =
                selected.imageFormat;
          }
          if (!std::strcmp(r, "format-transfer-dst-unsupported"))
            transferFeature = false;
          if (!std::strcmp(r, "null-image"))
            selected.vk = VK_NULL_HANDLE;
          if (!std::strcmp(r, "null-image-view"))
            selected.imageView = VK_NULL_HANDLE;
          if (!std::strcmp(r, "null-framebuffer"))
            framebuffer.vk = VK_NULL_HANDLE;
          if (!std::strcmp(r, "null-render-pass"))
            framebuffer.renderPass = VK_NULL_HANDLE;
          if (!std::strcmp(r, "layout-not-color-write"))
            selected.textureLayout = RenderTextureLayout::PRESENT;
          if (!std::strcmp(r, "missing-graphics-stage"))
            selected.barrierStages = RenderBarrierStage::NONE;
          if (!std::strcmp(r, "legacy-requires-external-image"))
            selected.ownership = true;
          if (!std::strcmp(r, "legacy-requires-single-sample"))
            selected.desc.multisampling.sampleCount = 8;
          rejectBeforeRecording = true;
          RenderRect rect = {0, 0, 32, 32};
          list.clearColor(selectedIndex, RenderColor(),
                          !std::strcmp(r, "partial-rectangles") ? &rect
                                                                : nullptr,
                          !std::strcmp(r, "partial-rectangles") ? 1 : 0);
          REQUIRE(false);
        }
        list.clearColor(selectedIndex, RenderColor(0.2f, 0.3f, 0.4f, 0.0f),
                        nullptr, 0);
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                               \
    !TOOIE_VULKAN_TRANSFER_CLEAR_TEST
#if defined(TOOIE_VULKAN_CLEAR_BARRIER_TEST) && TOOIE_VULKAN_CLEAR_BARRIER_TEST
        REQUIRE(commands == std::vector<char>({'M'}));
#else
        REQUIRE(commands == std::vector<char>({'M', 'B'}));
        REQUIRE(selectedPass ==
                framebuffer.colorClearRenderPasses[selectedIndex]);
        REQUIRE(selectedValue.float32[0] == 0.2f &&
                selectedValue.float32[3] == 0.0f);
#endif
#elif defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                             \
    TOOIE_VULKAN_TRANSFER_CLEAR_TEST
        REQUIRE(commands == (samples == 1 ? std::vector<char>({'I', 'K', 'I'})
                                          : std::vector<char>({'B'})));
#else
        REQUIRE(commands == std::vector<char>({'I', 'K', 'I'}));
        REQUIRE(selectedImage == selected.vk &&
                imageLayouts.at(selected.vk) ==
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        REQUIRE(imageLayouts.at(count == 1 ? second.vk : first.vk) ==
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        REQUIRE(selectedValue.float32[0] == 0.2f &&
                selectedValue.float32[1] == 0.3f &&
                selectedValue.float32[2] == 0.4f &&
                selectedValue.float32[3] == 0.0f);
#endif
        REQUIRE(selected.textureLayout == RenderTextureLayout::COLOR_WRITE);
        REQUIRE(selected.barrierStages == RenderBarrierStage::GRAPHICS);
        commands.clear();
        list.drawInstanced(3, 1, 0, 0);
        REQUIRE(commands.back() == 'D');
        commands.clear();
        list.clearColor(selectedIndex, RenderColor(), nullptr, 0);
#if !defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST)
        REQUIRE(commands == std::vector<char>({'E', 'I', 'K', 'I'}));
#endif
#if !defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                              \
    !defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK)
        RenderRect rectangles[] = {{0, 0, 32, 32}, {8, 16, 40, 48}};
        if (drawReject) {
          if (!std::strcmp(drawReject, "inverted-x"))
            rectangles[1] = {40, 16, 8, 48};
          if (!std::strcmp(drawReject, "inverted-y"))
            rectangles[1] = {8, 48, 40, 16};
          if (!std::strcmp(drawReject, "zero-width"))
            rectangles[1] = {8, 16, 8, 48};
          if (!std::strcmp(drawReject, "zero-height"))
            rectangles[1] = {8, 16, 40, 16};
          if (!std::strcmp(drawReject, "outside-malformed"))
            rectangles[1] = {-1, -3, -5, -1};
          if (!std::strcmp(drawReject, "framebuffer-extent-unrepresentable"))
            framebuffer.width = uint32_t(INT32_MAX) + 1;
          if (!std::strcmp(drawReject, "empty-framebuffer"))
            framebuffer.width = 0;
          if (!std::strcmp(drawReject, "unsupported-draw-format"))
            framebuffer.colorAttachmentFormats[selectedIndex] =
                VK_FORMAT_R8G8B8A8_UINT;
          if (!std::strcmp(drawReject, "format-color-attachment-unsupported"))
            colorFeature = false;
          if (!std::strcmp(drawReject, "missing-color-attachment-usage"))
            selected.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
          if (!std::strcmp(drawReject, "null-attachment-view"))
            framebuffer.colorAttachmentImageViews[selectedIndex] =
                VK_NULL_HANDLE;
          if (!std::strcmp(drawReject, "layout-not-color-write"))
            selected.textureLayout = RenderTextureLayout::PRESENT;
          if (!std::strcmp(drawReject, "missing-graphics-stage"))
            selected.barrierStages = RenderBarrierStage::NONE;
          if (!std::strcmp(drawReject, "outside-unsupported-format")) {
            rectangles[0] = rectangles[1] = {-8, -8, -1, -1};
            framebuffer.colorAttachmentFormats[selectedIndex] =
                VK_FORMAT_R8G8B8A8_UINT;
          }
          if (!std::strcmp(drawReject, "outside-unsafe-layout")) {
            rectangles[0] = rectangles[1] = {640, 480, 650, 490};
            selected.textureLayout = RenderTextureLayout::PRESENT;
          }
          rejectBeforeRecording = true;
          commands.clear();
          list.clearColor(selectedIndex, RenderColor(), rectangles, 2);
          REQUIRE(false);
        }
        // Capture a caller with multiple viewports/scissors, a non-default
        // depth bias, and partially overlapping push updates in an incompatible
        // range.
        RenderViewport viewports[] = {{1, 2, 100, 200, 0, 1},
                                      {3, 4, 50, 60, 0, 1}};
        RenderRect scissors[] = {{1, 2, 99, 199}, {3, 4, 49, 59}};
        list.setViewports(viewports, 2);
        list.setScissors(scissors, 2);
        list.setDepthBias(2.0f, 0.0f, 3.0f);
        const VkPipeline callerPipeline = handle<VkPipeline>();
        list.graphicsPipeline = callerPipeline;
        RenderPushConstantRange range(0, 0, 0, 48,
                                      RenderShaderStageFlag::PIXEL);
        VulkanPipelineLayout layout(
            &device, RenderPipelineLayoutDesc(&range, 1, nullptr, 0));
        list.setGraphicsPipelineLayout(&layout);
        uint8_t constants[48];
        std::memset(constants, 17, sizeof(constants));
        list.setGraphicsPushConstants(0, constants, 0, sizeof(constants));
        uint32_t patch = 0x76543210;
        list.setGraphicsPushConstants(0, &patch, 4, sizeof(patch));
        std::memcpy(constants + 4, &patch, 4);
        commands.clear();
        clearDrawScissors.clear();
        const unsigned pipelineCount = drawPipelineCreates;
        // Fully outside input must not even create a helper or restart a pass.
        const RenderRect outside[] = {{-40, -30, -10, -1},
                                      {640, 0, 641, 20},
                                      {0, 480, 10, 481},
                                      {0, -2, 2, 0},
                                      {INT32_MIN, INT32_MIN, -1, -1}};
        const VkPipeline beforeOutside = boundPipeline;
        const bool passBeforeOutside = inPass;
        list.clearColor(selectedIndex, RenderColor(), outside,
                        std::size(outside));
        REQUIRE(commands.empty() && clearDrawScissors.empty() &&
                drawPipelineCreates == pipelineCount &&
                inPass == passBeforeOutside);
        REQUIRE(boundPipeline == beforeOutside &&
                list.graphicsPipeline == callerPipeline &&
                boundViewports.size() == 2 && boundViewports[0].x == 1 &&
                boundScissors.size() == 2 && boundScissors[0].offset.x == 1);
        REQUIRE(boundDepthBias[0] == 2 && boundDepthBias[2] == 3 &&
                pushed.size() == 48 &&
                std::memcmp(pushed.data(), constants, 48) == 0);
        list.clearColor(selectedIndex, RenderColor(0.2f, 0.3f, 0.4f, 0.0f),
                        rectangles, 2);
        REQUIRE(commands == (inPass ? std::vector<char>({'M', 'B', 'D', 'D'})
                                    : std::vector<char>{}));
        REQUIRE(drawPipelineCreates == pipelineCount + 1);
        REQUIRE(clearDrawScissors.size() == 2 &&
                clearDrawScissors[1].offset.x == 8 &&
                clearDrawScissors[1].extent.height == 32);
        REQUIRE(boundPipeline == callerPipeline && boundViewports.size() == 2 &&
                boundViewports[0].x == 1 && boundScissors.size() == 2 &&
                boundScissors[0].offset.x == 1);
        REQUIRE(boundDepthBias[0] == 2 && boundDepthBias[2] == 3);
        REQUIRE(pushed.size() == 48 &&
                std::memcmp(pushed.data(), constants, 48) == 0);
        REQUIRE(list.activeGraphicsPipelineLayout == &layout &&
                list.graphicsPushConstantWrites.size() == 2);
        // An empty intersection also preserves a render pass already in flight.
        REQUIRE(inPass);
        commands.clear();
        clearDrawScissors.clear();
        list.clearColor(selectedIndex, RenderColor(), outside,
                        std::size(outside));
        REQUIRE(commands.empty() && clearDrawScissors.empty() && inPass &&
                drawPipelineCreates == pipelineCount + 1 &&
                boundPipeline == callerPipeline && pushed.size() == 48 &&
                std::memcmp(pushed.data(), constants, 48) == 0);
        commands.clear();
        list.clearColor(selectedIndex, RenderColor(), rectangles, 1);
        REQUIRE(commands == std::vector<char>({'E', 'M', 'B', 'D'}) &&
                drawPipelineCreates == pipelineCount + 1);
        struct ClipCase {
          RenderRect original, expected;
        };
        for (const ClipCase &clip : std::vector<ClipCase>{
                 {{-5, -7, 9, 13}, {0, 0, 9, 13}},
                 {{630, 470, 700, 700}, {630, 470, 640, 480}},
                 {{639, 479, 641, 481}, {639, 479, 640, 480}},
                 {{INT32_MIN, 1, 1, 2}, {0, 1, 1, 2}},
                 {{0, 0, 641, 1}, {0, 0, 640, 1}},
                 {{INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX},
                  {0, 0, 640, 480}}}) {
          // An outside rectangle in the same batch must contribute no draw.
          const RenderRect mixed[] = {outside[0], clip.original, outside[1]};
          commands.clear();
          clearDrawScissors.clear();
          list.clearColor(selectedIndex, RenderColor(), mixed,
                          std::size(mixed));
          REQUIRE(commands == std::vector<char>({'E', 'M', 'B', 'D'}) &&
                  drawPipelineCreates == pipelineCount + 1 &&
                  clearDrawScissors.size() == 1);
          const VkRect2D &scissor = clearDrawScissors[0];
          REQUIRE(scissor.offset.x == clip.expected.left &&
                  scissor.offset.y == clip.expected.top &&
                  scissor.extent.width ==
                      uint32_t(clip.expected.right - clip.expected.left) &&
                  scissor.extent.height ==
                      uint32_t(clip.expected.bottom - clip.expected.top));
          // draw() independently checks pushed rectangle == Vulkan scissor.
          REQUIRE(boundPipeline == callerPipeline &&
                  boundViewports.size() == 2 && boundViewports[0].x == 1 &&
                  boundScissors.size() == 2 && boundScissors[0].offset.x == 1 &&
                  boundDepthBias[0] == 2 && boundDepthBias[2] == 3 &&
                  pushed.size() == 48 &&
                  std::memcmp(pushed.data(), constants, 48) == 0);
        }
        std::printf(
            "PASS: clipped/empty/mixed rectangles, shared scissor/push bounds, "
            "no transfer, state restoration samples=%u attachment=%u\n",
            samples, selectedIndex);
        list.graphicsPushConstantWrites.clear();
        list.activeGraphicsPipelineLayout = nullptr;
#endif
#if defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK) &&                   \
    TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK
        RenderRect rect = {0, 0, 32, 32};
        const bool wasInPass = inPass;
        commands.clear();
        list.clearColor(selectedIndex, RenderColor(), &rect, 1);
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                               \
    TOOIE_VULKAN_TRANSFER_CLEAR_TEST
        REQUIRE(commands ==
                (wasInPass ? std::vector<char>{} : std::vector<char>{'B'}));
#else
        REQUIRE(commands == (wasInPass ? std::vector<char>{'C'}
                                       : std::vector<char>{'B', 'C'}));
#endif
#endif
        list.setFramebuffer(nullptr);
#if !defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                              \
    defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK) &&                   \
    TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK
        // Guard whole-image semantics: missing usage/features, views,
        // mismatched extents/format, mips/layers, integers, and linear tiling
        // must not transfer.
        for (unsigned rejection = 0; rejection < 9; ++rejection) {
          auto savedDesc = selected.desc;
          auto savedFormat = selected.imageFormat;
          auto savedUsage = selected.imageUsage;
          if (rejection == 0)
            selected.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
          if (rejection == 1)
            transferFeature = false;
          if (rejection == 2)
            framebuffer.colorAttachmentUsesBaseView[selectedIndex] = false;
          if (rejection == 3)
            selected.desc.width = 1280;
          if (rejection == 4)
            selected.imageFormat = VK_FORMAT_R8G8B8A8_UINT;
          if (rejection == 5)
            selected.desc.mipLevels = 2;
          if (rejection == 6)
            selected.desc.arraySize = 2;
          if (rejection == 7)
            selected.desc.textureArrangement =
                RenderTextureArrangement::ROW_MAJOR;
          if (rejection == 8)
            framebuffer.colorAttachmentFormats[selectedIndex] =
                VK_FORMAT_R8G8B8A8_SRGB;
          list.setFramebuffer(&framebuffer);
          commands.clear();
          list.clearColor(selectedIndex, RenderColor(), nullptr, 0);
          REQUIRE(commands == std::vector<char>({'B', 'C'}));
          list.setFramebuffer(nullptr);
          selected.desc = savedDesc;
          selected.imageFormat = savedFormat;
          selected.imageUsage = savedUsage;
          transferFeature = true;
          framebuffer.colorAttachmentUsesBaseView[selectedIndex] = true;
          framebuffer.colorAttachmentFormats[selectedIndex] = savedFormat;
        }
#endif
      }
      selected.ownership = false;
    }
#if !defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                              \
    !defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK)
  testAttachmentClearDomain(device, list);
#endif
  testDepthClearDomain(device, list);
  REQUIRE(destroyedPasses == createdPasses && framebufferExtents.empty());
  list.begin();
  REQUIRE(list.graphicsPipeline == VK_NULL_HANDLE &&
          list.graphicsViewports.empty() && list.graphicsScissors.empty() &&
          list.graphicsPushConstantWrites.empty() &&
          !list.graphicsDepthBiasSet);
  list.end();
  VulkanCommandSemaphore first(&device), second(&device);
  RenderCommandSemaphore *waits[] = {&first, &second};
  const RenderCommandList *lists[] = {&list};
  queue.executeCommandLists(lists, 1, waits, 2, nullptr, 0, nullptr);
  VulkanCommandFence fence(&device);
  queue.waitForCommandFence(&fence);
  std::puts("PASS: recorded clear commands, layouts, scopes, compatibility, "
            "guards, MSAA, semaphore masks, five-second timeout");
}
