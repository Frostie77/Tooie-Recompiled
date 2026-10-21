// Executes the production Android clear code through a host Vulkan driver.
#include "android_vulkan_draw_test_shaders.h"
#include "plume_vulkan.h"
#include "plume_vulkan_clear_shaders.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace plume;
#include "android_vulkan_validation_common.h"
// Independent expected image: test membership of each framebuffer pixel in the
// original rectangle, rather than reimplementing the backend's min/max
// clipping.
static void paintOriginalRectangle(std::vector<RenderColor> &pixels,
                                   unsigned width, unsigned height,
                                   RenderRect original, RenderColor color) {
  for (unsigned y = 0; y < height; ++y)
    for (unsigned x = 0; x < width; ++x)
      if (int32_t(x) >= original.left && int32_t(x) < original.right &&
          int32_t(y) >= original.top && int32_t(y) < original.bottom)
        pixels[y * width + x] = color;
}

static void clippedPixels(VulkanDevice &device, RenderCommandQueue *queue,
                          RenderCommandList *list, RenderCommandFence *fence) {
  auto vertex = device.createShader(ColorClearVertex, sizeof(ColorClearVertex),
                                    "main", RenderShaderFormat::SPIRV);
  auto fragment =
      device.createShader(OrdinaryDrawFragment, sizeof(OrdinaryDrawFragment),
                          "main", RenderShaderFormat::SPIRV);
  RenderPushConstantRange range(0, 0, 0, sizeof(RenderColor),
                                RenderShaderStageFlag::PIXEL);
  auto layout = device.createPipelineLayout(
      RenderPipelineLayoutDesc(&range, 1, nullptr, 0));
  for (auto format :
       {RenderFormat::R8G8B8A8_UNORM, RenderFormat::B8G8R8A8_UNORM}) {
    VkImageFormatProperties limits = {};
    REQUIRE(
        vkGetPhysicalDeviceImageFormatProperties(
            device.physicalDevice,
            format == RenderFormat::R8G8B8A8_UNORM ? VK_FORMAT_R8G8B8A8_UNORM
                                                   : VK_FORMAT_B8G8R8A8_UNORM,
            VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT,
            0, &limits) == VK_SUCCESS);
    REQUIRE((limits.sampleCounts &
             (VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)) ==
            (VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT));
    for (uint32_t samples : {1u, 2u, 4u}) {
      if (!(limits.sampleCounts & samples)) {
        std::printf("SKIP: clipped pixel execution samples=%u format=%u "
                    "unsupported by host image configuration\n",
                    samples, unsigned(format));
        continue;
      }
      for (RenderRect extent :
           {RenderRect{0, 0, 1, 1}, RenderRect{0, 0, 16, 16},
            RenderRect{0, 0, 320, 241}}) {
        const int32_t width = extent.right, height = extent.bottom;
        const RenderMultisampling ms(samples);
        auto target = device.createTexture(
            RenderTextureDesc::ColorTarget(width, height, format, ms));
        auto resolved = device.createTexture(
            RenderTextureDesc::ColorTarget(width, height, format));
        const RenderTexture *attachment = target.get();
        auto framebuffer =
            device.createFramebuffer(RenderFramebufferDesc(&attachment, 1));
        REQUIRE(framebuffer->getWidth() == uint32_t(width) &&
                framebuffer->getHeight() == uint32_t(height));
        RenderGraphicsPipelineDesc desc;
        desc.vertexShader = vertex.get();
        desc.pixelShader = fragment.get();
        desc.pipelineLayout = layout.get();
        desc.multisampling = ms;
        desc.renderTargetCount = 1;
        desc.renderTargetFormat[0] = format;
        desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        desc.depthClipEnabled = true;
        auto pipeline = device.createGraphicsPipeline(desc);
        REQUIRE(static_cast<VulkanGraphicsPipeline *>(pipeline.get())->vk !=
                VK_NULL_HANDLE);
        const RenderColor background(0.1f, 0.2f, 0.3f, 0.7f),
            clear(0.8f, 0.3f, 0.1f, 0.0f);
        const RenderRect cases[] = {
            {-3, 0, 1, height},
            {0, -3, width, 1},
            {width - 1, 0, width + 3, height},
            {0, height - 1, width, height + 3},
            {-4, -5, width + 4, height + 5},
            {width - 1, height - 1, width + 1, height + 1},
            {-8, 0, -1, height},
            {0, -8, width, -1},
            {width, 0, width + 3, height},
            {0, height, width, height + 3},
            {-1, 0, 0, height},
            {0, -1, width, 0},
            {INT32_MIN, 0, 1, height},
            {0, INT32_MIN, width, 1},
            {width - 1, 0, INT32_MAX, height},
            {0, height - 1, width, INT32_MAX},
            {INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX},
            {INT32_MAX - 1, 0, INT32_MAX, height}};
        auto check = [&](const RenderRect *rectangles, uint32_t count) {
          std::vector<RenderColor> expected(width * height, background);
          list->begin();
          list->barriers(RenderBarrierStage::GRAPHICS,
                         RenderTextureBarrier(
                             target.get(), RenderTextureLayout::COLOR_WRITE));
          list->setFramebuffer(framebuffer.get());
          list->setPipeline(pipeline.get());
          list->setGraphicsPipelineLayout(layout.get());
          list->setGraphicsPushConstants(0, &background);
          list->setViewports(RenderViewport(0, 0, width, height));
          list->setScissors(extent);
          list->drawInstanced(3, 1, 0,
                              0); // Background independent of all clear paths.
          list->clearColor(0, clear, rectangles, count);
          for (uint32_t i = 0; i < count; ++i)
            paintOriginalRectangle(expected, width, height, rectangles[i],
                                   clear);
          RenderTexture *source = target.get();
          if (samples > 1) {
            RenderTextureBarrier barriers[] = {
                {target.get(), RenderTextureLayout::RESOLVE_SOURCE},
                {resolved.get(), RenderTextureLayout::RESOLVE_DEST}};
            list->barriers(RenderBarrierStage::COPY, barriers, 2);
            list->resolveTexture(resolved.get(), target.get());
            source = resolved.get();
          }
          expectPixels(
              readImage(device, list, queue, fence, source, width, height, 4),
              expected, format);
        };
        for (const RenderRect &rectangle : cases)
          check(&rectangle, 1);
        const RenderRect mixed[] = {cases[6], cases[0], cases[8]};
        check(mixed, std::size(mixed));
        std::printf("PASS: %zu clipped/fully-outside/mixed pixel cases "
                    "extent=%dx%d format=%u samples=%u; original-domain "
                    "RGB/alpha and outside pixels preserved\n",
                    std::size(cases) + 1, width, height, unsigned(format),
                    samples);
      }
    }
  }
}
static void partialPixels(VulkanDevice &device, RenderCommandQueue *queue,
                          RenderCommandList *list, RenderCommandFence *fence) {
  auto vertex = device.createShader(ColorClearVertex, sizeof(ColorClearVertex),
                                    "main", RenderShaderFormat::SPIRV);
  auto fragment =
      device.createShader(OrdinaryDrawFragment, sizeof(OrdinaryDrawFragment),
                          "main", RenderShaderFormat::SPIRV);
  RenderPushConstantRange range(
      0, 0, 0, 48,
      RenderShaderStageFlag::PIXEL); // Deliberately incompatible with helper's
                                     // 32-byte range.
  auto layout = device.createPipelineLayout(
      RenderPipelineLayoutDesc(&range, 1, nullptr, 0));
  for (RenderFormat format :
       {RenderFormat::R8G8B8A8_UNORM, RenderFormat::B8G8R8A8_UNORM,
        RenderFormat::R16G16B16A16_UNORM, RenderFormat::R16G16B16A16_FLOAT,
        RenderFormat::R32G32B32A32_FLOAT}) {
    for (uint32_t samples : {1u, 4u}) {
      RenderMultisampling ms;
      ms.sampleCount = samples;
      auto target = device.createTexture(
          RenderTextureDesc::ColorTarget(16, 16, format, ms));
      auto resolved =
          device.createTexture(RenderTextureDesc::ColorTarget(16, 16, format));
      const RenderTexture *color = target.get();
      auto framebuffer =
          device.createFramebuffer(RenderFramebufferDesc(&color, 1));
      RenderGraphicsPipelineDesc desc;
      desc.pipelineLayout = layout.get();
      desc.vertexShader = vertex.get();
      desc.pixelShader = fragment.get();
      desc.multisampling = ms;
      desc.renderTargetCount = 1;
      desc.renderTargetFormat[0] = format;
      desc.depthClipEnabled = true;
      desc.dynamicDepthBiasEnabled = true;
      desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
      auto pipeline = device.createGraphicsPipeline(desc);
      REQUIRE(static_cast<VulkanGraphicsPipeline *>(pipeline.get())->vk !=
              VK_NULL_HANDLE);
      const RenderColor background(0.1f, 0.2f, 0.3f, 0.7f),
          red(0.8f, 0.2f, 0.4f, 0.0f), green(0.2f, 0.7f, 0.3f, 0.5f),
          blue(0.1f, 0.3f, 0.9f, 0.2f);
      std::vector<RenderColor> expected(16 * 16, background);
      list->begin();
      list->barriers(
          RenderBarrierStage::GRAPHICS,
          RenderTextureBarrier(target.get(), RenderTextureLayout::COLOR_WRITE));
      list->setFramebuffer(framebuffer.get());
      list->setPipeline(pipeline.get());
      list->setGraphicsPipelineLayout(layout.get());
      RenderViewport viewport(0, 0, 16, 16);
      RenderRect scissor = {0, 0, 16, 16};
      list->setViewports(&viewport, 1);
      list->setScissors(&scissor, 1);
      list->setDepthBias(0, 0, 0);
      float constants[12] = {
          background.r, background.g, background.b, background.a, 9, 8, 7, 6};
      list->setGraphicsPushConstants(0, constants);
      list->drawInstanced(
          3, 1, 0,
          0); // Background is an ordinary draw, independently of clear code.
      constants[0] = blue.r;
      constants[1] = 0.0f;
      constants[2] = blue.b;
      constants[3] = blue.a;
      list->setGraphicsPushConstants(0, constants);
      list->setGraphicsPushConstants(
          0, &blue.g, 4, 4); // Partial write must survive both clears.
      viewport = RenderViewport(12, 12, 3, 3);
      scissor = {12, 12, 16, 16};
      list->setViewports(&viewport, 1);
      list->setScissors(&scissor, 1);
      RenderRect rectangles[] = {{0, 1, 7, 9}, {5, 5, 11, 13}};
      list->clearColor(0, red, rectangles, 2);
      for (auto rect : rectangles)
        paint(expected, 16, rect, red);
      RenderRect inner = {2, 3, 6, 6};
      list->clearColor(0, green, &inner, 1);
      paint(expected, 16, inner, green);
      // No rebind: validates pipeline, viewport, scissor and push restoration.
      list->drawInstanced(3, 1, 0, 0);
      paint(expected, 16, {12, 12, 15, 15}, blue);
      RenderTexture *source = target.get();
      if (samples > 1) {
        RenderTextureBarrier barriers[] = {
            {target.get(), RenderTextureLayout::RESOLVE_SOURCE},
            {resolved.get(), RenderTextureLayout::RESOLVE_DEST}};
        list->barriers(RenderBarrierStage::COPY, barriers, 2);
        list->resolveTexture(resolved.get(), target.get());
        source = resolved.get();
      }
      expectPixels(readImage(device, list, queue, fence, source, 16, 16,
                             pixelStride(format)),
                   expected, format);
      std::printf("PASS: partial rectangles/overlap/alpha/preserved "
                  "outside/state restoration format=%u samples=%u\n",
                  unsigned(format), samples);
    }
  }
}

static void attachmentViewPixels(VulkanDevice &device,
                                 RenderCommandQueue *queue,
                                 RenderCommandList *list,
                                 RenderCommandFence *fence) {
  auto desc =
      RenderTextureDesc::ColorTarget(16, 16, RenderFormat::R8G8B8A8_UNORM);
  desc.arraySize = 2;
  desc.mipLevels = 2;
  auto texture = device.createTexture(desc);
  RenderTextureViewDesc viewDesc =
      RenderTextureViewDesc::Texture2D(desc.format);
  viewDesc.mipSlice = 1;
  viewDesc.mipLevels = 1;
  viewDesc.arrayIndex = 1;
  viewDesc.arraySize = 1;
  auto view = texture->createTextureView(viewDesc);
  const RenderTextureView *attachment = view.get();
  RenderFramebufferDesc fbDesc;
  fbDesc.colorAttachmentViews = &attachment;
  fbDesc.colorAttachmentsCount = 1;
  auto framebuffer = device.createFramebuffer(fbDesc);
  REQUIRE(framebuffer->getWidth() == 8 && framebuffer->getHeight() == 8);
  const RenderColor background(0.1f, 0.2f, 0.3f, 0.7f),
      clear(0.8f, 0.3f, 0.1f, 0.0f);
  list->begin();
  list->barriers(
      RenderBarrierStage::COPY,
      RenderTextureBarrier(texture.get(), RenderTextureLayout::COPY_DEST));
  // Fixture initialization only: whole image/siblings are deliberate here.
  auto *vkTexture = static_cast<VulkanTexture *>(texture.get());
  auto *vkList = static_cast<VulkanCommandList *>(list);
  VkClearColorValue color;
  std::memcpy(color.float32, background.rgba, sizeof(color.float32));
  vkCmdClearColorImage(vkList->vk, vkTexture->vk,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1,
                       &vkTexture->imageSubresourceRange);
  list->barriers(
      RenderBarrierStage::GRAPHICS,
      RenderTextureBarrier(texture.get(), RenderTextureLayout::COLOR_WRITE));
  list->setFramebuffer(framebuffer.get());
  RenderRect rectangle = {1, 1, 6, 5};
  list->clearColor(0, clear, &rectangle, 1);
  const RenderRect oversized = {-3, 2, 12, 7};
  const RenderColor clippedColor(0.2f, 0.7f, 0.4f, 0.5f);
  list->clearColor(0, clippedColor, &oversized, 1);
  for (unsigned mip = 0; mip < 2; ++mip) {
    for (unsigned layer = 0; layer < 2; ++layer) {
      const unsigned width = 16 >> mip;
      std::vector<RenderColor> expected(width * width, background);
      if (mip == 1 && layer == 1) {
        paint(expected, width, rectangle, clear);
        paintOriginalRectangle(expected, width, width, oversized, clippedColor);
      }
      expectPixels(readImage(device, list, queue, fence, texture.get(), width,
                             width, 4, mip, layer),
                   expected, desc.format);
      if (mip != 1 || layer != 1)
        list->begin();
    }
  }
  std::puts("PASS: actual view mip=1/layer=1 extent=8x8, all sibling "
            "mips/layers preserved");
}
static void multipleAttachmentPixels(VulkanDevice &device,
                                     RenderCommandQueue *queue,
                                     RenderCommandList *list,
                                     RenderCommandFence *fence) {
  for (uint32_t samples : {1u, 4u}) {
    RenderMultisampling ms;
    ms.sampleCount = samples;
    auto first = device.createTexture(RenderTextureDesc::ColorTarget(
        16, 16, RenderFormat::R8G8B8A8_UNORM, ms));
    auto second = device.createTexture(RenderTextureDesc::ColorTarget(
        16, 16, RenderFormat::B8G8R8A8_UNORM, ms));
    auto depth = device.createTexture(
        RenderTextureDesc::DepthTarget(16, 16, RenderFormat::D32_FLOAT, ms));
    // Use an explicit view on MSAA as well as single-sample targets.
    RenderTextureViewDesc viewDesc =
        RenderTextureViewDesc::Texture2D(RenderFormat::B8G8R8A8_UNORM);
    viewDesc.mipLevels = viewDesc.arraySize = 1;
    auto view = second->createTextureView(viewDesc);
    const RenderTexture *colors[] = {first.get(), second.get()};
    const RenderTextureView *views[] = {nullptr, view.get()};
    RenderFramebufferDesc fbDesc(colors, 2, depth.get());
    fbDesc.colorAttachmentViews = views;
    auto framebuffer = device.createFramebuffer(fbDesc);
    auto vertex =
        device.createShader(ColorClearVertex, sizeof(ColorClearVertex), "main",
                            RenderShaderFormat::SPIRV);
    auto fragment =
        device.createShader(OrdinaryDrawFragment, sizeof(OrdinaryDrawFragment),
                            "main", RenderShaderFormat::SPIRV);
    RenderPushConstantRange range(0, 0, 0, 16, RenderShaderStageFlag::PIXEL);
    auto layout = device.createPipelineLayout(
        RenderPipelineLayoutDesc(&range, 1, nullptr, 0));
    RenderGraphicsPipelineDesc desc;
    desc.vertexShader = vertex.get();
    desc.pixelShader = fragment.get();
    desc.pipelineLayout = layout.get();
    desc.depthClipEnabled = true;
    desc.multisampling = ms;
    desc.renderTargetCount = 2;
    desc.renderTargetFormat[0] = RenderFormat::R8G8B8A8_UNORM;
    desc.renderTargetFormat[1] = RenderFormat::B8G8R8A8_UNORM;
    desc.renderTargetBlend[0] = desc.renderTargetBlend[1] =
        RenderBlendDesc::Copy();
    desc.renderTargetBlend[1].renderTargetWriteMask = 0;
    desc.depthTargetFormat = RenderFormat::D32_FLOAT;
    desc.depthEnabled = true;
    desc.depthFunction = RenderComparisonFunction::EQUAL;
    auto pipeline = device.createGraphicsPipeline(desc);
    const RenderColor background(0.1f, 0.2f, 0.3f, 0.7f),
        clear(0.8f, 0.3f, 0.1f, 0), blue(0.2f, 0.4f, 0.9f, 0.5f);
    list->begin();
    list->barriers(
        RenderBarrierStage::COPY,
        RenderTextureBarrier(depth.get(), RenderTextureLayout::COPY_DEST));
    auto *vkList = static_cast<VulkanCommandList *>(list);
    auto *vkDepth = static_cast<VulkanTexture *>(depth.get());
    VkClearDepthStencilValue depthValue = {0.25f, 0};
    vkCmdClearDepthStencilImage(
        vkList->vk, vkDepth->vk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &depthValue, 1, &vkDepth->imageSubresourceRange);
    RenderTextureBarrier barriers[] = {
        {first.get(), RenderTextureLayout::COLOR_WRITE},
        {second.get(), RenderTextureLayout::COLOR_WRITE},
        {depth.get(), RenderTextureLayout::DEPTH_WRITE}};
    list->barriers(RenderBarrierStage::GRAPHICS, barriers, 3);
    list->setFramebuffer(framebuffer.get());
    // Explicit full rectangle is a draw, never a whole-image transfer, because
    // the second attachment uses a non-base view.
    RenderRect entire = {0, 0, 16, 16};
    list->clearColor(0, background);
    list->clearColor(1, background, &entire, 1);
    list->setPipeline(pipeline.get());
    list->setGraphicsPipelineLayout(layout.get());
    list->setGraphicsPushConstants(0, &blue);
    RenderViewport viewports[] = {RenderViewport(12, 12, 3, 3, 0.25f, 0.25f),
                                  RenderViewport(12, 12, 3, 3, 0.25f, 0.25f)};
    RenderRect scissors[] = {{12, 12, 15, 15}, {12, 12, 15, 15}};
    list->setViewports(viewports, 2);
    list->setScissors(scissors, 2);
    RenderRect rectangle = {1, 2, 7, 10};
    list->clearColor(1, clear, &rectangle, 1);
    list->drawInstanced(3, 1, 0,
                        0); // EQUAL passes only if depth stayed at 0.25.
    for (unsigned index = 0; index < 2; ++index) {
      auto resolved = device.createTexture(RenderTextureDesc::ColorTarget(
          16, 16,
          index == 0 ? RenderFormat::R8G8B8A8_UNORM
                     : RenderFormat::B8G8R8A8_UNORM));
      RenderTexture *source = index == 0 ? first.get() : second.get();
      if (samples > 1) {
        RenderTextureBarrier resolveBarriers[] = {
            {source, RenderTextureLayout::RESOLVE_SOURCE},
            {resolved.get(), RenderTextureLayout::RESOLVE_DEST}};
        list->barriers(RenderBarrierStage::COPY, resolveBarriers, 2);
        list->resolveTexture(resolved.get(), source);
        source = resolved.get();
      }
      std::vector<RenderColor> expected(16 * 16, background);
      paint(expected, 16, index == 0 ? scissors[0] : rectangle,
            index == 0 ? blue : clear);
      expectPixels(readImage(device, list, queue, fence, source, 16, 16, 4),
                   expected,
                   index == 0 ? RenderFormat::R8G8B8A8_UNORM
                              : RenderFormat::B8G8R8A8_UNORM);
      if (index == 0)
        list->begin();
    }
    std::printf("PASS: selected attachment=1 explicit view, other colour/depth "
                "preserved, samples=%u\n",
                samples);
  }
}

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  HostVulkanValidation validation;
  auto &interface = validation.interface;
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
#if !defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST)
    partialPixels(device, queue.get(), list.get(), fence.get());
    clippedPixels(device, queue.get(), list.get(), fence.get());
    attachmentViewPixels(device, queue.get(), list.get(), fence.get());
    multipleAttachmentPixels(device, queue.get(), list.get(), fence.get());
#endif
  }
}
