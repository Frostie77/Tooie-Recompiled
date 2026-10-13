// Recording regression test: runs the production Android code with mocked
// Vulkan entry points. The host native-window declarations are test doubles,
// not an Android runtime.
#include "plume_vulkan.h"
#include <cstdio>
#include <cstdlib>
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
static size_t handleId = 1;
template <class T> T handle() { return reinterpret_cast<T>(handleId++); }
static std::vector<std::vector<VkAttachmentDescription>> passDescs;
static std::vector<char> commands;
static VkRenderPass selectedPass;
static VkClearValue selectedValue;
static unsigned destroyedPasses = 0;
static bool simulateTimeout = false;
static VkResult enumerate(VkInstance, uint32_t *count, VkPhysicalDevice *) {
  *count = 0;
  return VK_SUCCESS;
}
static VkResult createPass(VkDevice, const VkRenderPassCreateInfo *p,
                           const VkAllocationCallbacks *, VkRenderPass *out) {
  passDescs.emplace_back(p->pAttachments, p->pAttachments + p->attachmentCount);
  *out = handle<VkRenderPass>();
  return VK_SUCCESS;
}
static void destroyPass(VkDevice, VkRenderPass, const VkAllocationCallbacks *) {
  destroyedPasses++;
}
static VkResult createFb(VkDevice, const VkFramebufferCreateInfo *,
                         const VkAllocationCallbacks *, VkFramebuffer *out) {
  *out = handle<VkFramebuffer>();
  return VK_SUCCESS;
}
static void destroyFb(VkDevice, VkFramebuffer, const VkAllocationCallbacks *) {}
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
  commands.push_back('B');
  selectedPass = p->renderPass;
  if (p->clearValueCount) {
    REQUIRE(p->clearValueCount == 2);
    selectedValue = p->pClearValues[1];
  }
  REQUIRE(p->renderArea.extent.width == 640);
  REQUIRE(p->renderArea.extent.height == 480);
}
static void endPass(VkCommandBuffer) { commands.push_back('E'); }
static void barrier(VkCommandBuffer, VkPipelineStageFlags src,
                    VkPipelineStageFlags dst, VkDependencyFlags, uint32_t count,
                    const VkMemoryBarrier *p, uint32_t,
                    const VkBufferMemoryBarrier *, uint32_t images,
                    const VkImageMemoryBarrier *) {
  commands.push_back('M');
  REQUIRE(count == 1 && images == 0);
  REQUIRE(src & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  REQUIRE(dst & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  REQUIRE(p->srcAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  REQUIRE(p->dstAccessMask & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
}
static void clearAttachments(VkCommandBuffer, uint32_t,
                             const VkClearAttachment *, uint32_t,
                             const VkClearRect *) {
  commands.push_back('C');
}
static void draw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t) {
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
  REQUIRE(count == 1);
  REQUIRE(p->waitSemaphoreCount == 2);
  for (unsigned i = 0; i < 2; i++)
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
int main(int argc, char **) {
  simulateTimeout = argc > 1;
  VulkanInterface
      interface; // No surface/device: the API below is a recording mock.
  vkEnumeratePhysicalDevices = enumerate;
  VulkanDevice device(&interface, "");
  vkCreateRenderPass = createPass;
  vkDestroyRenderPass = destroyPass;
  vkCreateFramebuffer = createFb;
  vkDestroyFramebuffer = destroyFb;
  vkCreateCommandPool = createPool;
  vkDestroyCommandPool = destroyPool;
  vkAllocateCommandBuffers = allocate;
  vkFreeCommandBuffers = freeBuffers;
  vkCmdBeginRenderPass = beginPass;
  vkCmdEndRenderPass = endPass;
  vkCmdPipelineBarrier = barrier;
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
  for (uint32_t samples : {1u, 8u}) {
    VulkanTexture color0, color1, depth;
    color0.desc =
        RenderTextureDesc::ColorTarget(640, 480, RenderFormat::R8G8B8A8_UNORM);
    color1.desc = color0.desc;
    color0.desc.multisampling.sampleCount = samples;
    color1.desc.multisampling.sampleCount = samples;
    depth.desc =
        RenderTextureDesc::DepthTarget(640, 480, RenderFormat::D32_FLOAT);
    depth.desc.multisampling.sampleCount = samples;
    const RenderTexture *colors[] = {&color0, &color1};
    size_t base = passDescs.size();
    {
      VulkanFramebuffer framebuffer(&device,
                                    RenderFramebufferDesc(colors, 2, &depth));
      REQUIRE(passDescs.size() == base + 3);
      for (size_t variant = 0; variant < 3; variant++) {
        auto &a = passDescs[base + variant];
        REQUIRE(a.size() == 3);
        REQUIRE(a[2].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD);
        for (size_t i = 0; i < 2; i++) {
          REQUIRE(a[i].samples == samples);
          REQUIRE(a[i].loadOp == ((variant == i + 1)
                                      ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                      : VK_ATTACHMENT_LOAD_OP_LOAD));
          REQUIRE(a[i].initialLayout ==
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
          REQUIRE(a[i].finalLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
          REQUIRE(a[i].storeOp == VK_ATTACHMENT_STORE_OP_STORE);
        }
      }
      list.setFramebuffer(&framebuffer);
      commands.clear();
      list.clearColor(1, RenderColor(0.2f, 0.3f, 0.4f, 0.0f), nullptr, 0);
      REQUIRE(commands == std::vector<char>({'M', 'B'}));
      REQUIRE(selectedPass == framebuffer.colorClearRenderPasses[1]);
      REQUIRE(selectedValue.color.float32[0] == 0.2f &&
              selectedValue.color.float32[3] == 0.0f);
      list.drawInstanced(3, 1, 0, 0);
      REQUIRE(commands.back() == 'D');
      commands.clear();
      list.clearColor(1, RenderColor(), nullptr, 0);
      REQUIRE(commands == std::vector<char>({'E', 'M', 'B'}));
      RenderRect rect = {0, 0, 32, 32};
      commands.clear();
      list.clearColor(1, RenderColor(), &rect, 1);
      REQUIRE(
          commands ==
          std::vector<char>({'C'})); // partial clears keep their API semantics.
      list.setFramebuffer(nullptr);
    }
  }
  REQUIRE(destroyedPasses == 6);
  VulkanCommandSemaphore first(&device), second(&device);
  RenderCommandSemaphore *waits[] = {&first, &second};
  const RenderCommandList *lists[] = {&list};
  queue.executeCommandLists(lists, 1, waits, 2, nullptr, 0, nullptr);
  VulkanCommandFence fence(&device);
  queue.waitForCommandFence(&fence);
  std::puts(
      "PASS: 1x/8x clears, attachment preservation, pass restart/draw, partial "
      "fallback, pass destruction, multi-semaphore waits, five-second timeout");
}
