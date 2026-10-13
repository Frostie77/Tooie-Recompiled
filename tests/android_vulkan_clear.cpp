// Recording tests of production Android code, not GPU execution. Synthetic
// images describe the launcher's state; Vulkan entry points check every
// command.
#include "plume_vulkan.h"
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
extern "C" int32_t ANativeWindow_getWidth(ANativeWindow *) { return 1; }
extern "C" int32_t ANativeWindow_getHeight(ANativeWindow *) { return 1; }
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
static VkRenderPass selectedPass;
static VkClearColorValue selectedValue;
static VkImage selectedImage;
static bool inPass = false, simulateTimeout = false, invalidState = false;
static bool transferFeature = true;
static unsigned createdPasses = 0, destroyedPasses = 0, selectedIndex = 0;
static VkResult enumerate(VkInstance, uint32_t *count, VkPhysicalDevice *) {
  *count = 0;
  return VK_SUCCESS;
}
static void formatProperties(VkPhysicalDevice, VkFormat,
                             VkFormatProperties *out) {
  *out = {};
  out->optimalTilingFeatures =
      transferFeature ? VK_FORMAT_FEATURE_TRANSFER_DST_BIT : 0;
}
static VkResult createPass(VkDevice, const VkRenderPassCreateInfo *p,
                           const VkAllocationCallbacks *, VkRenderPass *out) {
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
  return VK_SUCCESS;
}
static void destroyFb(VkDevice, VkFramebuffer fb,
                      const VkAllocationCallbacks *) {
  framebufferPasses.erase(fb);
}
static VkResult createView(VkDevice, const VkImageViewCreateInfo *,
                           const VkAllocationCallbacks *, VkImageView *out) {
  *out = handle<VkImageView>();
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
  REQUIRE(inPass);
  commands.push_back('C');
}
static void draw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t) {
  REQUIRE(inPass);
  commands.push_back('D');
}
static VkResult createSem(VkDevice, const VkSemaphoreCreateInfo *,
                          const VkAllocationCallbacks *, VkSemaphore *out) {
  *out = handle<VkSemaphore>();
  return VK_SUCCESS;
}
static void destroySem(VkDevice, VkSemaphore, const VkAllocationCallbacks *) {}
static VkResult submit(VkQueue, uint32_t count, const VkSubmitInfo *p,
                       VkFence) {
  REQUIRE(count == 1 && p->waitSemaphoreCount == 2);
  for (unsigned i = 0; i < 2; ++i)
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
int main(int argc, char **argv) {
  simulateTimeout = argc > 1 && std::strcmp(argv[1], "--timeout") == 0;
  invalidState = argc > 1 && std::strcmp(argv[1], "--invalid-state") == 0;
  VulkanInterface interface; // A host loader may reject Android surface
                             // support. No GPU is used.
  vkEnumeratePhysicalDevices = enumerate;
  VulkanDevice device(&interface, "");
  vkGetPhysicalDeviceFormatProperties = formatProperties;
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
  for (uint32_t samples : {1u, 8u})
    for (unsigned count : {1u, 2u}) {
      VulkanTexture first, second, depth;
      RenderFormat format = samples == 8 ? RenderFormat::B8G8R8A8_UNORM
                                         : RenderFormat::R8G8B8A8_UNORM;
      initialize(first, device, samples, format);
      initialize(second, device, samples, format);
      depth.desc =
          RenderTextureDesc::DepthTarget(640, 480, RenderFormat::D32_FLOAT);
      depth.desc.multisampling.sampleCount = samples;
      const RenderTexture *colors[] = {&first, &second};
      selectedIndex = count - 1;
      auto &selected = count == 1 ? first : second;
      // Synthetic offscreen ownership metadata; never hand mock images to VMA.
      selected.ownership = samples == 8;
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
        if (invalidState)
          selected.textureLayout = RenderTextureLayout::PRESENT;
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
        list.setFramebuffer(nullptr);
#if !defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST)
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
  VulkanCommandSemaphore first(&device), second(&device);
  RenderCommandSemaphore *waits[] = {&first, &second};
  const RenderCommandList *lists[] = {&list};
  queue.executeCommandLists(lists, 1, waits, 2, nullptr, 0, nullptr);
  VulkanCommandFence fence(&device);
  queue.waitForCommandFence(&fence);
  std::puts("PASS: recorded clear commands, layouts, scopes, compatibility, "
            "guards, MSAA, semaphore masks, five-second timeout");
}
