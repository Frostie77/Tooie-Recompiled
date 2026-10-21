// Shared production VI pipeline creation; exercised by presentation readback tests.
#pragma once
#include "plume_render_interface.h"
#if defined(__ANDROID__)
#include "../../../../src/android_debug_log.h"
#endif
namespace RT64 {
inline std::unique_ptr<plume::RenderPipeline> CreateVideoInterfacePipeline(plume::RenderDevice *device, plume::RenderFormat format,
                                                                           const plume::RenderShader *vertex,
                                                                           const plume::RenderShader *pixel,
                                                                           const plume::RenderPipelineLayout *layout) {
    plume::RenderGraphicsPipelineDesc desc;
    desc.vertexShader = vertex;
    desc.pixelShader = pixel;
    desc.renderTargetFormat[0] = format;
    desc.renderTargetBlend[0] = plume::RenderBlendDesc::Copy();
    desc.renderTargetCount = 1;
    desc.pipelineLayout = layout;
#if defined(__ANDROID__)
    if (format != plume::RenderFormat::R8G8B8A8_UNORM && format != plume::RenderFormat::B8G8R8A8_UNORM) {
        tooie_android_debug_log("VI pipeline rejected: unsupported presentation format");
        std::_Exit(EXIT_FAILURE);
    }
    // Full-screen presentation needs no depth-clamp device feature.
    desc.depthClipEnabled = true;
    char record[128];
    std::snprintf(record, sizeof(record), "VI pipeline: format=%u samples=1", unsigned(format));
    tooie_android_debug_log(record);
#endif
    return device->createGraphicsPipeline(desc);
}
} // namespace RT64
