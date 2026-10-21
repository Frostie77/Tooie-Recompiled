// Actual production HLSL and pipeline factories, with CPU expected pixel
// values.
#include "../lib/RecompFrontend/recompui/src/renderer/ui_presentation.h"
#include "../lib/rt64/src/render/rt64_vi_pipeline.h"
#include "FullScreenVS.hlsl.spirv.h"
#include "InterfacePS.hlsl.spirv.h"
#include "InterfaceVS.hlsl.spirv.h"
#include "VideoInterfacePSPixel.hlsl.spirv.h"
#include "VideoInterfacePSRegular.hlsl.spirv.h"
#include "android_vulkan_validation_common.h"
#include <array>
#include <cstddef>

static RenderColor blend(RenderColor source, RenderColor destination,
                         bool premultiplied) {
  RenderColor result;
  for (unsigned c = 0; c < 3; ++c)
    result.rgba[c] = source.rgba[c] * (premultiplied ? 1 : source.a) +
                     destination.rgba[c] * (1 - source.a);
  result.a = source.a + destination.a * (1 - source.a);
  return result;
}
static void verifyPipeline(RenderPipeline *pipeline, RenderFormat format,
                           uint32_t samples) {
  auto *vkPipeline = static_cast<VulkanGraphicsPipeline *>(pipeline);
  REQUIRE(vkPipeline->vk != VK_NULL_HANDLE &&
          vkPipeline->renderPass != VK_NULL_HANDLE);
  // Compatibility is checked by core validation on each real draw below.
  REQUIRE(format == RenderFormat::R8G8B8A8_UNORM ||
          format == RenderFormat::B8G8R8A8_UNORM);
  REQUIRE(samples == 1 || samples == 4);
}
static void clear(RenderCommandList *list, RenderTexture *texture,
                  RenderFramebuffer *framebuffer, RenderColor color) {
  list->barriers(
      RenderBarrierStage::GRAPHICS,
      RenderTextureBarrier(texture, RenderTextureLayout::COLOR_WRITE));
  list->setFramebuffer(framebuffer);
  list->clearColor(0, color);
}
static void viPixels(VulkanDevice &device, RenderCommandQueue *queue,
                     RenderCommandList *list, RenderCommandFence *fence) {
  auto vertex =
      device.createShader(FullScreenVSBlobSPIRV, sizeof(FullScreenVSBlobSPIRV),
                          "VSMain", RenderShaderFormat::SPIRV);
  auto regular = device.createShader(VideoInterfacePSRegularBlobSPIRV,
                                     sizeof(VideoInterfacePSRegularBlobSPIRV),
                                     "PSMain", RenderShaderFormat::SPIRV);
  auto pixel = device.createShader(VideoInterfacePSPixelBlobSPIRV,
                                   sizeof(VideoInterfacePSPixelBlobSPIRV),
                                   "PSMain", RenderShaderFormat::SPIRV);
  struct Constants {
    float video[2];
    float texture[2];
    float gamma;
  } constants = {{1, 1}, {1, 1}, 1};
  const RenderColor color(0.8f, 0.2f, 0.1f, 0.35f);
  for (auto targetFormat :
       {RenderFormat::R8G8B8A8_UNORM, RenderFormat::B8G8R8A8_UNORM})
    for (auto sourceFormat :
         {RenderFormat::R8G8B8A8_UNORM, RenderFormat::B8G8R8A8_UNORM})
      for (unsigned variant = 0; variant < 3; ++variant) {
        RenderSamplerDesc samplerDesc;
        samplerDesc.minFilter = samplerDesc.magFilter =
            variant == 0 ? RenderFilter::NEAREST : RenderFilter::LINEAR;
        samplerDesc.addressU = samplerDesc.addressV =
            RenderTextureAddressMode::CLAMP;
        auto sampler = device.createSampler(samplerDesc);
        RenderDescriptorSetBuilder descriptors;
        descriptors.begin();
        const unsigned input = descriptors.addTexture(1);
        descriptors.addImmutableSampler(2, sampler.get());
        descriptors.end();
        auto descriptorSet = descriptors.create(&device);
        RenderPipelineLayoutBuilder layoutBuilder;
        layoutBuilder.begin();
        layoutBuilder.addPushConstant(0, 0, sizeof(constants),
                                      RenderShaderStageFlag::PIXEL);
        layoutBuilder.addDescriptorSet(descriptors);
        layoutBuilder.end();
        auto layout = layoutBuilder.create(&device);
        auto source = device.createTexture(
            RenderTextureDesc::ColorTarget(1, 1, sourceFormat));
        auto target = device.createTexture(
            RenderTextureDesc::ColorTarget(16, 16, targetFormat));
        const RenderTexture *inputAttachment = source.get(),
                            *outputAttachment = target.get();
        auto sourceFb = device.createFramebuffer(
            RenderFramebufferDesc(&inputAttachment, 1));
        auto targetFb = device.createFramebuffer(
            RenderFramebufferDesc(&outputAttachment, 1));
        auto pipeline = RT64::CreateVideoInterfacePipeline(
            &device, targetFb->getColorAttachmentFormat(0), vertex.get(),
            variant == 2 ? pixel.get() : regular.get(), layout.get());
        verifyPipeline(pipeline.get(), targetFormat, 1);
        descriptorSet->setTexture(input, source.get(),
                                  RenderTextureLayout::SHADER_READ);
        list->begin();
        clear(list, source.get(), sourceFb.get(), color);
        list->barriers(RenderBarrierStage::GRAPHICS,
                       RenderTextureBarrier(source.get(),
                                            RenderTextureLayout::SHADER_READ));
        clear(list, target.get(), targetFb.get(), RenderColor(0, 0, 0, 0));
        list->setPipeline(pipeline.get());
        list->setGraphicsPipelineLayout(layout.get());
        list->setGraphicsDescriptorSet(descriptorSet.get(), 0);
        list->setGraphicsPushConstants(0, &constants);
        list->setViewports(RenderViewport(0, 0, 16, 16));
        list->setScissors(RenderRect{0, 0, 16, 16});
        list->drawInstanced(3, 1, 0, 0);
        expectPixels(
            readImage(device, list, queue, fence, target.get(), 16, 16, 4),
            std::vector<RenderColor>(256,
                                     RenderColor(color.r, color.g, color.b, 1)),
            targetFormat);
        std::printf("PASS: production VI variant=%u source=%u target=%u RGB "
                    "order and opaque alpha\n",
                    variant, unsigned(sourceFormat), unsigned(targetFormat));
      }
}
struct UiVertex {
  float position[2];
  uint8_t color[4];
  float uv[2];
};
struct UiConstants {
  float matrix[16];
  float translation[2];
};
static void uiPixels(VulkanDevice &device, RenderCommandQueue *queue,
                     RenderCommandList *list, RenderCommandFence *fence) {
  auto vertex =
      device.createShader(InterfaceVSBlobSPIRV, sizeof(InterfaceVSBlobSPIRV),
                          "VSMain", RenderShaderFormat::SPIRV);
  auto pixel =
      device.createShader(InterfacePSBlobSPIRV, sizeof(InterfacePSBlobSPIRV),
                          "PSMain", RenderShaderFormat::SPIRV);
  RenderSamplerDesc samplerDesc;
  samplerDesc.minFilter = samplerDesc.magFilter = RenderFilter::LINEAR;
  samplerDesc.addressU = samplerDesc.addressV = RenderTextureAddressMode::CLAMP;
  auto sampler = device.createSampler(samplerDesc);
  RenderDescriptorSetBuilder frameDescriptors, textureDescriptors;
  frameDescriptors.begin();
  frameDescriptors.addImmutableSampler(1, sampler.get());
  frameDescriptors.addConstantBuffer(3);
  frameDescriptors.end();
  textureDescriptors.begin();
  const unsigned textureIndex = textureDescriptors.addTexture(2);
  textureDescriptors.end();
  auto frameSet = frameDescriptors.create(&device);
  auto textureSet = textureDescriptors.create(&device);
  auto resolvedSet = textureDescriptors.create(&device);
  RenderPipelineLayoutBuilder layoutBuilder;
  layoutBuilder.begin(false, true);
  layoutBuilder.addPushConstant(0, 0, sizeof(UiConstants),
                                RenderShaderStageFlag::VERTEX);
  layoutBuilder.addDescriptorSet(frameDescriptors);
  layoutBuilder.addDescriptorSet(textureDescriptors);
  layoutBuilder.end();
  auto layout = layoutBuilder.create(&device);
  const RenderInputSlot slot(0, sizeof(UiVertex),
                             RenderInputSlotClassification::PER_VERTEX_DATA);
  const RenderInputElement elements[] = {
      {"POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0,
       offsetof(UiVertex, position)},
      {"COLOR", 0, 1, RenderFormat::R8G8B8A8_UNORM, 0,
       offsetof(UiVertex, color)},
      {"TEXCOORD", 0, 2, RenderFormat::R32G32_FLOAT, 0,
       offsetof(UiVertex, uv)}};
  const UiVertex vertices[] = {{{-1, 1}, {255, 255, 255, 255}, {0, 0}},
                               {{-1, -3}, {255, 255, 255, 255}, {0, 2}},
                               {{3, 1}, {255, 255, 255, 255}, {2, 0}}};
  auto vertexBuffer = device.createBuffer(RenderBufferDesc::UploadBuffer(
      sizeof(vertices), RenderBufferFlag::VERTEX));
  auto *mapped = vertexBuffer->map();
  REQUIRE(mapped);
  std::memcpy(mapped, vertices, sizeof(vertices));
  vertexBuffer->unmap();
  REQUIRE(vmaFlushAllocation(
              device.allocator,
              static_cast<VulkanBuffer *>(vertexBuffer.get())->allocation, 0,
              VK_WHOLE_SIZE) == VK_SUCCESS);
  RenderVertexBufferView vertexView(vertexBuffer.get(), sizeof(vertices));
  UiConstants constants = {};
  for (unsigned i = 0; i < 4; ++i)
    constants.matrix[i * 4 + i] = 1;
  RenderGraphicsPipelineDesc base;
  base.vertexShader = vertex.get();
  base.pixelShader = pixel.get();
  base.pipelineLayout = layout.get();
  base.inputSlots = &slot;
  base.inputSlotsCount = 1;
  base.inputElements = elements;
  base.inputElementsCount = 3;
  recompui::UiPresentationPipelines cache;
  const RenderColor foreground(0.8f, 0.2f, 0.1f, 0.5f),
      scene(0.1f, 0.6f, 0.3f, 0.75f);
  auto bind = [&](RenderPipeline *pipeline, RenderDescriptorSet *input) {
    list->setPipeline(pipeline);
    list->setGraphicsPipelineLayout(layout.get());
    list->setGraphicsPushConstants(0, &constants);
    list->setGraphicsDescriptorSet(frameSet.get(), 0);
    list->setGraphicsDescriptorSet(input, 1);
    list->setVertexBuffers(0, &vertexView, 1, &slot);
    list->setViewports(RenderViewport(0, 0, 16, 16));
    list->setScissors(RenderRect{0, 0, 16, 16});
    list->drawInstanced(3, 1, 0, 0);
  };
  for (auto targetFormat :
       {RenderFormat::R8G8B8A8_UNORM, RenderFormat::B8G8R8A8_UNORM}) {
    auto target = device.createTexture(
        RenderTextureDesc::ColorTarget(16, 16, targetFormat));
    const RenderTexture *output = target.get();
    auto targetFb = device.createFramebuffer(RenderFramebufferDesc(&output, 1));
    for (auto sourceFormat :
         {RenderFormat::R8G8B8A8_UNORM, RenderFormat::B8G8R8A8_UNORM}) {
      auto source = device.createTexture(
          RenderTextureDesc::ColorTarget(1, 1, sourceFormat));
      const RenderTexture *input = source.get();
      auto sourceFb =
          device.createFramebuffer(RenderFramebufferDesc(&input, 1));
      textureSet->setTexture(textureIndex, source.get(),
                             RenderTextureLayout::SHADER_READ);
      list->begin();
      clear(list, source.get(), sourceFb.get(), foreground);
      list->barriers(
          RenderBarrierStage::GRAPHICS,
          RenderTextureBarrier(source.get(), RenderTextureLayout::SHADER_READ));
      clear(list, target.get(), targetFb.get(), scene);
      auto *pipeline = cache.get(&device, base, targetFb.get(), false);
      verifyPipeline(pipeline, targetFormat, 1);
      bind(pipeline, textureSet.get());
      expectPixels(
          readImage(device, list, queue, fence, target.get(), 16, 16, 4),
          std::vector<RenderColor>(256, blend(foreground, scene, false)),
          targetFormat);
      std::printf("PASS: production UI direct source=%u target=%u RGB order "
                  "and straight alpha blend\n",
                  unsigned(sourceFormat), unsigned(targetFormat));
    }
    for (uint32_t samples : {1u, 4u}) {
      const auto offscreenFormat = recompui::UiOffscreenFormat;
      auto offscreen = device.createTexture(RenderTextureDesc::ColorTarget(
          16, 16, offscreenFormat, RenderMultisampling(samples)));
      auto resolved = device.createTexture(
          RenderTextureDesc::ColorTarget(16, 16, offscreenFormat));
      auto source = device.createTexture(
          RenderTextureDesc::ColorTarget(1, 1, RenderFormat::R8G8B8A8_UNORM));
      const RenderTexture *offscreenColor = offscreen.get(),
                          *sourceColor = source.get();
      auto offscreenFb =
          device.createFramebuffer(RenderFramebufferDesc(&offscreenColor, 1));
      auto sourceFb =
          device.createFramebuffer(RenderFramebufferDesc(&sourceColor, 1));
      textureSet->setTexture(textureIndex, source.get(),
                             RenderTextureLayout::SHADER_READ);
      auto offscreenPipeline = recompui::CreateUiPipeline(
          &device, base, offscreenFb->getColorAttachmentFormat(0),
          RenderMultisampling(samples), recompui::UiPipelinePath::Offscreen);
      verifyPipeline(offscreenPipeline.get(), offscreenFormat, samples);
      list->begin();
      clear(list, source.get(), sourceFb.get(), foreground);
      list->barriers(
          RenderBarrierStage::GRAPHICS,
          RenderTextureBarrier(source.get(), RenderTextureLayout::SHADER_READ));
      clear(list, offscreen.get(), offscreenFb.get(), RenderColor(0, 0, 0, 0));
      bind(offscreenPipeline.get(), textureSet.get());
      RenderTexture *sampled = offscreen.get();
      if (samples > 1) {
        RenderTextureBarrier barriers[] = {
            {offscreen.get(), RenderTextureLayout::RESOLVE_SOURCE},
            {resolved.get(), RenderTextureLayout::RESOLVE_DEST}};
        list->barriers(RenderBarrierStage::COPY, barriers, 2);
        list->resolveTexture(resolved.get(), offscreen.get());
        sampled = resolved.get();
      }
      list->barriers(
          RenderBarrierStage::GRAPHICS,
          RenderTextureBarrier(sampled, RenderTextureLayout::SHADER_READ));
      resolvedSet->setTexture(textureIndex, sampled,
                              RenderTextureLayout::SHADER_READ);
      clear(list, target.get(), targetFb.get(), scene);
      auto *composite = cache.get(&device, base, targetFb.get(), true);
      verifyPipeline(composite, targetFormat, 1);
      bind(composite, resolvedSet.get());
      const RenderColor premultiplied =
          blend(foreground, RenderColor(0, 0, 0, 0), false);
      expectPixels(
          readImage(device, list, queue, fence, target.get(), 16, 16, 4),
          std::vector<RenderColor>(256, blend(premultiplied, scene, true)),
          targetFormat);
      std::printf("PASS: production UI BGRA offscreen samples=%u "
                  "resolve/composite target=%u premultiplied RGB and alpha\n",
                  samples, unsigned(targetFormat));
    }
  }
}
int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  HostVulkanValidation validation;
  VulkanDevice device(&validation.interface, "llvmpipe");
  REQUIRE(device.vk != VK_NULL_HANDLE &&
          device.physicalDeviceProperties.deviceType ==
              VK_PHYSICAL_DEVICE_TYPE_CPU);
  auto queue = device.createCommandQueue(RenderCommandListType::DIRECT);
  auto list = queue->createCommandList();
  auto fence = device.createCommandFence();
  viPixels(device, queue.get(), list.get(), fence.get());
  uiPixels(device, queue.get(), list.get(), fence.get());
}
