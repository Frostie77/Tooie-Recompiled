// Recording tests of production Android code, not GPU execution. Synthetic
// images describe the launcher's state; Vulkan entry points check every
// command.
#include "../lib/RecompFrontend/recompui/src/renderer/ui_presentation.h"
#include "../lib/rt64/src/hle/rt64_vulkan_queue_diagnostic.h"
#include "../lib/rt64/src/render/rt64_vi_pipeline.h"
#include "plume_vulkan.h"
#include "plume_vulkan_clear_shaders.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
static bool transferFeature = true;
static const char *clearRejectionCase = nullptr;
static bool rejectBeforeRecording = false;
static VkImageCreateInfo expectedImageInfo = {};
static VkImageFormatProperties imageLimits = {};
static VkResult imageQueryResult = VK_SUCCESS;
static unsigned imageQueries = 0, swapchainCreates = 0;
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
    REQUIRE(format == expectedSwapchainFormat &&
            type == VK_IMAGE_TYPE_2D && tiling == VK_IMAGE_TILING_OPTIMAL &&
            flags == 0);
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
                           const VkAllocationCallbacks *) {}
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
  REQUIRE((p->imageUsage & ~surfaceUsage) == 0);
  REQUIRE(p->imageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  *out = handle<VkSwapchainKHR>();
  return VK_SUCCESS;
}
static void destroySwapchain(VkDevice, VkSwapchainKHR,
                             const VkAllocationCallbacks *) {}
static VkResult swapchainImages(VkDevice, VkSwapchainKHR, uint32_t *count,
                                VkImage *out) {
  *count = 2;
  if (out) {
    out[0] = handle<VkImage>();
    out[1] = handle<VkImage>();
    imageLayouts[out[0]] = imageLayouts[out[1]] = VK_IMAGE_LAYOUT_UNDEFINED;
    swapImageFormats[out[0]] = swapImageFormats[out[1]] =
        createdSwapchainFormat;
  }
  return VK_SUCCESS;
}
static VkResult acquireImage(VkDevice, VkSwapchainKHR, uint64_t,
                             VkSemaphore semaphore, VkFence, uint32_t *out) {
  ++acquireCalls;
  acquired = semaphore;
  *out = 0;
  return acquireFails ? VK_ERROR_OUT_OF_DATE_KHR : VK_SUCCESS;
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
  REQUIRE(p->width == 640 && p->height == 480 && p->layers == 1);
  REQUIRE(p->attachmentCount == passes.at(p->renderPass).attachments.size());
  *out = handle<VkFramebuffer>();
  framebufferPasses[*out] = p->renderPass;
  const auto &attachments = passes.at(p->renderPass).attachments;
  for (unsigned i = 0; i < p->attachmentCount; ++i)
    if (actualViewFormats.count(p->pAttachments[i]))
      REQUIRE(actualViewFormats.at(p->pAttachments[i]) ==
              attachments.at(i).format);
  return VK_SUCCESS;
}
static void destroyFb(VkDevice, VkFramebuffer fb,
                      const VkAllocationCallbacks *) {
  framebufferPasses.erase(fb);
}
static VkResult createView(VkDevice, const VkImageViewCreateInfo *info,
                           const VkAllocationCallbacks *, VkImageView *out) {
  REQUIRE(info->format != VK_FORMAT_UNDEFINED);
  if (swapImageFormats.count(info->image))
    REQUIRE(info->format == swapImageFormats.at(info->image));
  *out = handle<VkImageView>();
  actualViewFormats[*out] = info->format;
  return VK_SUCCESS;
}
static void destroyView(VkDevice, VkImageView, const VkAllocationCallbacks *) {}
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
  REQUIRE(p->renderArea.extent.width == 640 &&
          p->renderArea.extent.height == 480);
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
static void clearAttachments(VkCommandBuffer, uint32_t,
                             const VkClearAttachment *, uint32_t,
                             const VkClearRect *) {
  REQUIRE(!rejectBeforeRecording);
  REQUIRE(inPass);
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
  device.independentBlendSupported = true;
  if (argc > 1 && std::strncmp(argv[1], "--pipeline-reject=", 18) == 0) {
    RenderGraphicsPipelineDesc base;
    presentationFeatures &= ~VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    rejectBeforeRecording = true;
    auto rejected = recompui::CreateUiPipeline(
        &device, base,
        std::strcmp(argv[1] + 18, "bgra") == 0
            ? RenderFormat::B8G8R8A8_UNORM
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
  for (uint32_t samples : {1u, 8u})
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
      selected.ownership = samples == 8;
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
          if (!std::strcmp(drawReject, "rectangle-out-of-bounds"))
            rectangles[1].right = 641;
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
        commands.clear();
        list.clearColor(selectedIndex, RenderColor(), rectangles, 1);
        REQUIRE(commands == std::vector<char>({'E', 'M', 'B', 'D'}) &&
                drawPipelineCreates == pipelineCount + 1);
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
  REQUIRE(destroyedPasses == createdPasses);
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
