#include "android_vulkan_validation_common.h"
#include "descriptor-pool-test.spirv.h"
#include "render/rt64_descriptor_sets.h"
#include <algorithm>
#include "plume_vulkan_descriptor_probe.h"

static PFN_vkCreateDescriptorPool originalCreatePool;
static VkResult declaredCapacityPool(VkDevice device,const VkDescriptorPoolCreateInfo *info,const VkAllocationCallbacks *callbacks,VkDescriptorPool *out) {
    if(!(info->flags & VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT)) return originalCreatePool(device,info,callbacks,out);
    REQUIRE(info->poolSizeCount==1 && info->pPoolSizes[0].type==VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    VkDescriptorPoolSize size=info->pPoolSizes[0];size.descriptorCount=RT64::FramebufferRendererDescriptorTextureSet::UpperRange;
    VkDescriptorPoolCreateInfo copied=*info;copied.pPoolSizes=&size;
    return originalCreatePool(device,&copied,callbacks,out);
}

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
    const bool capacityControl=argc==2 && std::strcmp(argv[1],"declared-pool")==0;
    const bool qualifiedControl=argc==2 && std::strcmp(argv[1],"qualified")==0;
    originalEnumerate=vkEnumerateDeviceExtensionProperties;
    if(coreOnly) vkEnumerateDeviceExtensionProperties=coreExtensions;
    {
        VulkanDevice device(&validation.interface,"llvmpipe"); REQUIRE(device.vk);
        static_assert(RT64::FramebufferRendererDescriptorTextureSet::UpperRange==8192);
        REQUIRE(runVulkanDescriptorAllocationProbe(device.vk,device.descriptorIndexingFeatures));
        // Exercise the production Android reservation policy on a real CPU
        // Vulkan driver. Only selection metadata is synthetic, never device
        // handles/features/limits/API calls; this is not Adreno verification.
        if(qualifiedControl) {
            device.physicalDeviceProperties.vendorID=0x5143;
            std::strcpy(device.physicalDeviceProperties.deviceName,"Adreno (TM) 740");
            device.driverProperties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            device.driverProperties.driverID=VK_DRIVER_ID_QUALCOMM_PROPRIETARY;
        }
        originalCreatePool=vkCreateDescriptorPool;
        if(capacityControl) vkCreateDescriptorPool=declaredCapacityPool;
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
        std::unique_ptr<RT64::FramebufferRendererDescriptorTextureSet> previous;
        for(uint32_t requested:{0u,1u,2u,32u,1024u,8192u}) {
            auto next=std::make_unique<RT64::FramebufferRendererDescriptorTextureSet>(&device,requested);
            auto &textures=*next;
            const uint32_t allocated=std::max(requested,1u),selected=allocated-1;
            auto *vkTextures=static_cast<VulkanDescriptorSet *>(textures.get());
            REQUIRE(vkTextures->allocatedCounts[0]==allocated);
            REQUIRE(vkTextures->reservedUpdateAfterBindSlots==(qualifiedControl?textureLayout.UpperRange:allocated));
            const uint64_t oldSlots=previous?static_cast<VulkanDescriptorSet *>(previous->get())->reservedUpdateAfterBindSlots:0;
            REQUIRE(device.liveUpdateAfterBindSlots==oldSlots+vkTextures->reservedUpdateAfterBindSlots);
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
            std::printf("PASS: %s production framebuffer common/8192-image layouts requested=%u allocated=%u last-element RGB/alpha readback\n",coreOnly?"core-only":qualifiedControl?"qualified-policy-on-Lavapipe":capacityControl?"declared-pool":"extension",requested,allocated);
            // Allocate the new set while the preceding set is still alive, as
            // unique_ptr replacement in RT64 does. Prior GPU work is fenced.
            previous=std::move(next);
        }
        previous.reset();
        REQUIRE(device.liveUpdateAfterBindSlots==0);
        vkCreateDescriptorPool=originalCreatePool;
    }
    vkEnumerateDeviceExtensionProperties=originalEnumerate;
    REQUIRE(validationErrors==0);
}
