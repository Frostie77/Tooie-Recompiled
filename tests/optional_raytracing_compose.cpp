// Execute the production compose setup block with recording stand-ins. Shader
// bytes, the rest of ShaderLibrary setup and GPU execution are outside scope.
#include "plume_render_interface_types.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
namespace plume {
class RenderShader { public: unsigned id = 0; };
class RenderPipelineLayout {};
class RenderPipeline {};
}
using namespace plume;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"optional-compose failure: %s at %d\n",#x,__LINE__);std::exit(64); } } while (0)
struct UnsupportedBlend : std::runtime_error { using std::runtime_error::runtime_error; };
struct RecordingDevice {
    RenderDeviceCapabilities caps;
    unsigned features = 0xDC83; // Actual Redmi format-109 optimal features.
    unsigned shaders = 0, layouts = 0, pipelines = 0;
    RenderDeviceCapabilities getCapabilities() const { return caps; }
    std::unique_ptr<RenderShader> createShader(const char *entry, RenderShaderFormat format) {
        CHECK(std::strcmp(entry,"PSMain")==0 && format==RenderShaderFormat::SPIRV);
        ++shaders;auto s=std::make_unique<RenderShader>();s->id=1;return s;
    }
    std::unique_ptr<RenderPipeline> createGraphicsPipeline(const RenderGraphicsPipelineDesc &d) {
        ++pipelines;
        CHECK(d.renderTargetCount==1 && d.renderTargetFormat[0]==RenderFormat::R32G32B32A32_FLOAT);
        const auto &b=d.renderTargetBlend[0];
        const auto expected=RenderBlendDesc::AlphaBlend();
        CHECK(b.blendEnabled && b.srcBlend==expected.srcBlend && b.dstBlend==expected.dstBlend);
        CHECK(b.blendOp==expected.blendOp && b.srcBlendAlpha==expected.srcBlendAlpha);
        CHECK(b.dstBlendAlpha==expected.dstBlendAlpha && b.blendOpAlpha==expected.blendOpAlpha);
        CHECK(b.renderTargetWriteMask==expected.renderTargetWriteMask);
        CHECK(d.pipelineLayout && d.vertexShader && d.vertexShader->id==0);
        CHECK(d.pixelShader && d.pixelShader->id==1);
        if ((features & 0x180U)!=0x180U)
            throw UnsupportedBlend("recorded format=109 optimal=0xDC83 lacks attachment blend 0x100");
        return std::make_unique<RenderPipeline>();
    }
};
struct Samplers {};
struct RaytracingComposeDescriptorSet { explicit RaytracingComposeDescriptorSet(Samplers&) {} };
struct RenderPipelineLayoutBuilder {
    unsigned state=0;
    void begin() { CHECK(state==0);state=1; }
    void addDescriptorSet(const RaytracingComposeDescriptorSet&) { CHECK(state==1);state=2; }
    void end() { CHECK(state==2);state=3; }
    std::unique_ptr<RenderPipelineLayout> create(RecordingDevice *d) {
        CHECK(state==3);++d->layouts;return std::make_unique<RenderPipelineLayout>();
    }
};
#define CREATE_SHADER_INPUTS(dxil,spirv,msl,entry,format) entry,format
struct Fixture {
    Samplers samplerLibrary;
    struct { std::unique_ptr<RenderPipeline> pipeline;std::unique_ptr<RenderPipelineLayout> pipelineLayout; } compose;
    void setup(RecordingDevice *device) {
        // Exact production capability retrieval; the include below is copied
        // verbatim from setupCommonShaders by the driver.
#include "compose-capabilities.inc"
        const auto shaderFormat=RenderShaderFormat::SPIRV;
        auto fullScreenVertexShader=std::make_unique<RenderShader>();
        RenderPipelineLayoutBuilder layoutBuilder;
#include "compose-production.inc"
    }
};
int main() {
    unsigned cases=0;
    for (bool rt : {false,true}) for (bool blend : {false,true}) {
        RecordingDevice device;device.caps.raytracing=rt;
        if (blend) device.features|=0x100U;
        Fixture f;bool refused=false;
        try { f.setup(&device); }
        catch (const UnsupportedBlend &e) {
            refused=true;
            if (!(RT_ENABLED && rt && !blend)) {
                std::fprintf(stderr,"unexpected compose request on raster-only path: %s\n",e.what());return 64;
            }
        }
        const bool needed=RT_ENABLED && rt;
        CHECK(refused==(needed && !blend));
        CHECK(device.pipelines==unsigned(needed) && device.shaders==unsigned(needed));
        CHECK(device.layouts==unsigned(needed));
        CHECK(bool(f.compose.pipeline)==(needed && blend));
        CHECK(bool(f.compose.pipelineLayout)==needed);
        ++cases;
    }
    std::printf("PASS: %u production compose-setup cases with RT_ENABLED=%d; recorded Mali refusal, raster-only skip and retained supported-RT format/blend/shader descriptor.\n",cases,RT_ENABLED);
}
