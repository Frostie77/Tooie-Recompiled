// Android UI paths: straight-alpha draws into BGRA offscreen or presentation
// targets; premultiplied-alpha composition of the resolved offscreen image.
#pragma once
#include "../../../../../src/android_debug_log.h"
#include "plume_render_interface.h"
#include <array>
namespace recompui {
constexpr plume::RenderFormat UiOffscreenFormat = plume::RenderFormat::B8G8R8A8_UNORM;
enum class UiPipelinePath { Direct, Offscreen, Composite };
inline std::unique_ptr<plume::RenderPipeline> CreateUiPipeline(plume::RenderDevice *device, const plume::RenderGraphicsPipelineDesc &base,
                                                               plume::RenderFormat format, plume::RenderMultisampling multisampling,
                                                               UiPipelinePath path) {
    if (format != plume::RenderFormat::R8G8B8A8_UNORM && format != plume::RenderFormat::B8G8R8A8_UNORM) {
        tooie_android_debug_log("UI pipeline rejected: unsupported attachment format");
        std::_Exit(EXIT_FAILURE);
    }
    plume::RenderGraphicsPipelineDesc desc = base;
    desc.renderTargetFormat[0] = format;
    desc.renderTargetCount = 1;
    desc.multisampling = multisampling;
    desc.depthClipEnabled = true;
    desc.renderTargetBlend[0] = plume::RenderBlendDesc::AlphaBlend();
    if (path == UiPipelinePath::Composite)
        desc.renderTargetBlend[0].srcBlend = plume::RenderBlend::ONE;
    char record[160];
    std::snprintf(record, sizeof(record), "UI pipeline: path=%s format=%u samples=%u srcBlend=%u",
                  path == UiPipelinePath::Direct      ? "direct"
                  : path == UiPipelinePath::Offscreen ? "offscreen"
                                                      : "composite",
                  unsigned(format), multisampling.sampleCount, unsigned(desc.renderTargetBlend[0].srcBlend));
    tooie_android_debug_log(record);
    return device->createGraphicsPipeline(desc);
}
struct UiPresentationPipelines {
    std::array<std::unique_ptr<plume::RenderPipeline>, 4> pipelines;
    plume::RenderPipeline *get(plume::RenderDevice *device, const plume::RenderGraphicsPipelineDesc &base,
                               const plume::RenderFramebuffer *framebuffer, bool composite) {
        const plume::RenderFormat format = framebuffer ? framebuffer->getColorAttachmentFormat(0) : plume::RenderFormat::UNKNOWN;
        if (format != plume::RenderFormat::R8G8B8A8_UNORM && format != plume::RenderFormat::B8G8R8A8_UNORM) {
            tooie_android_debug_log("UI presentation rejected: missing or unsupported framebuffer format");
            std::_Exit(EXIT_FAILURE);
        }
        const size_t index = (format == plume::RenderFormat::R8G8B8A8_UNORM ? 0 : 1) + (composite ? 2 : 0);
        if (!pipelines[index])
            pipelines[index] = CreateUiPipeline(device, base, format, plume::RenderMultisampling(),
                                                composite ? UiPipelinePath::Composite : UiPipelinePath::Direct);
        return pipelines[index].get();
    }
};
} // namespace recompui
