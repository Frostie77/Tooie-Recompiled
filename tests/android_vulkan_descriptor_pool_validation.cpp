#include "android_vulkan_validation_common.h"
#include "descriptor-pool-test.spirv.h"
#include "render/rt64_descriptor_sets.h"
#include <algorithm>

// A Vulkan 1.2+ device may omit the promoted extension name. Keep the real
// device, features, allocation and command execution; filter only that name.
static PFN_vkEnumerateDeviceExtensionProperties originalEnumerate;
static VkResult coreExtensions(VkPhysicalDevice device, const char *layer, uint32_t *count, VkExtensionProperties *out) {
    uint32_t n=0;
    VkResult result=originalEnumerate(device,layer,&n,nullptr);
    if(result!=VK_SUCCESS) return result;
    std::vector<VkExtensionProperties> properties(n);
    result=originalEnumerate(device,layer,&n,properties.data());
    if(result!=VK_SUCCESS) return result;
    properties.resize(n);
    properties.erase(std::remove_if(properties.begin(),properties.end(),[](const auto &p) {
        return std::strcmp(p.extensionName,VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)==0;
    }),properties.end());
    if(!out) { *count=uint32_t(properties.size()); return VK_SUCCESS; }
    n=std::min(*count,uint32_t(properties.size()));
    std::copy_n(properties.data(),n,out); *count=n;
    return n<properties.size()?VK_INCOMPLETE:VK_SUCCESS;
}

int main(int argc,char **argv) {
    HostVulkanValidation validation;
    const bool coreOnly=argc==2 && std::strcmp(argv[1],"core-only")==0;
    originalEnumerate=vkEnumerateDeviceExtensionProperties;
    if(coreOnly) vkEnumerateDeviceExtensionProperties=coreExtensions;
    {
        VulkanDevice device(&validation.interface,"llvmpipe"); REQUIRE(device.vk);
        REQUIRE(device.descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount &&
            device.descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind);
        RT64::SamplerLibrary samplers;
        for(auto *set:{&samplers.linear,&samplers.nearest}) {
            for(auto *slot:{&set->wrapWrap,&set->wrapMirror,&set->wrapClamp,&set->mirrorWrap,&set->mirrorMirror,
                &set->mirrorClamp,&set->clampWrap,&set->clampMirror,&set->clampClamp}) *slot=device.createSampler(RenderSamplerDesc());
        }
        RT64::FramebufferRendererDescriptorCommonSet common(samplers,false,&device);
        RT64::FramebufferRendererDescriptorTextureSet textureLayout;
        RenderDescriptorSetBuilder output; output.begin(); auto resultIndex=output.addReadWriteTexture(0); output.end();
        RenderDescriptorSetDesc descriptions[]={common.builder.descriptorSetDesc,textureLayout.builder.descriptorSetDesc,output.descriptorSetDesc};
        RenderPushConstantRange push(0,0,0,sizeof(uint32_t),RenderShaderStageFlag::COMPUTE);
        auto layout=device.createPipelineLayout(RenderPipelineLayoutDesc(&push,1,descriptions,3));
        auto shader=device.createShader(DescriptorPoolTest,sizeof(DescriptorPoolTest),"main",RenderShaderFormat::SPIRV);
        RenderComputePipelineDesc pipelineDesc; pipelineDesc.pipelineLayout=layout.get(); pipelineDesc.computeShader=shader.get();
        auto pipeline=device.createComputePipeline(pipelineDesc);
        auto queue=device.createCommandQueue(RenderCommandListType::DIRECT);
        auto list=queue->createCommandList(); auto fence=device.createCommandFence();
        for(uint32_t requested:{0u,1u,2u,32u,1024u,8192u}) {
            RT64::FramebufferRendererDescriptorTextureSet textures(&device,requested);
            const uint32_t allocated=std::max(requested,1u),selected=allocated-1;
            REQUIRE(static_cast<VulkanDescriptorSet *>(textures.get())->allocatedCounts[0]==allocated);
            auto src=device.createTexture(RenderTextureDesc::ColorTarget(1,1,RenderFormat::R8G8B8A8_UNORM));
            auto dst=device.createTexture(RenderTextureDesc::Texture2D(1,1,1,RenderFormat::R8G8B8A8_UNORM,RenderTextureFlag::STORAGE));
            const RenderTexture *color=src.get(); auto framebuffer=device.createFramebuffer(RenderFramebufferDesc(&color,1));
            textures.setTexture(selected,src.get(),RenderTextureLayout::SHADER_READ);
            auto out=output.create(&device); out->setTexture(resultIndex,dst.get(),RenderTextureLayout::GENERAL);
            RenderColor expected(0.25f,0.5f,0.75f,0.625f);
            list->begin(); list->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(src.get(),RenderTextureLayout::COLOR_WRITE));
            list->setFramebuffer(framebuffer.get()); list->clearColor(0,expected); list->setFramebuffer(nullptr);
            RenderTextureBarrier barriers[]={RenderTextureBarrier(src.get(),RenderTextureLayout::SHADER_READ),RenderTextureBarrier(dst.get(),RenderTextureLayout::GENERAL)};
            list->barriers(RenderBarrierStage::COMPUTE,nullptr,0,barriers,2);
            list->setPipeline(pipeline.get()); list->setComputePipelineLayout(layout.get());
            list->setComputeDescriptorSet(common.get(),0); list->setComputeDescriptorSet(textures.get(),1); list->setComputeDescriptorSet(out.get(),2);
            list->setComputePushConstants(0,&selected); list->dispatch(1,1,1);
            auto pixels=readImage(device,list.get(),queue.get(),fence.get(),dst.get(),1,1,4);
            expectPixels(pixels,{expected},RenderFormat::R8G8B8A8_UNORM);
            std::printf("PASS: %s production framebuffer common/8192-image layouts requested=%u allocated=%u last-element RGB/alpha readback\n",coreOnly?"core-only":"extension",requested,allocated);
        }
    }
    vkEnumerateDeviceExtensionProperties=originalEnumerate;
    REQUIRE(validationErrors==0);
}
