// Real framebuffer descriptor preparation + production asynchronous uploader,
// consumed on CPU Vulkan. No ROM, Activity or presentation surface is needed.
#include "android_vulkan_validation_common.h"
#include "gpu-tiles-test.spirv.h"
#include "render/rt64_framebuffer_renderer.h"
#include <cstddef>

// This fixture provides only the shader-library container. It does not replace
// framebuffer preparation, buffer allocation/upload, Plume or shader consumers.
// Production VI/UI shaders/factories are checked by the presentation suite.
namespace RT64 {
ShaderLibrary::ShaderLibrary(bool hdr,bool resolve):usesHDR(hdr),usesHardwareResolve(resolve) {}
ShaderLibrary::~ShaderLibrary() {}
}

static PFN_vkUpdateDescriptorSets originalUpdate;
static PFN_vkCmdCopyBuffer originalCopy;
static PFN_vkCmdPipelineBarrier originalBarrier;
static VkBuffer watchedBuffer;
static VkDescriptorBufferInfo tileBinding;
static unsigned tileWrites, tileCopies, tileBefore, tileAfter;
static VkBufferCopy tileCopy;
static void update(VkDevice d,uint32_t n,const VkWriteDescriptorSet *w,uint32_t c,const VkCopyDescriptorSet *copies) {
    for(uint32_t i=0;i<n;++i) if(w[i].dstBinding==4 && w[i].descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        REQUIRE(w[i].descriptorCount==1 && w[i].pBufferInfo && w[i].pBufferInfo->buffer);
        tileBinding=*w[i].pBufferInfo; ++tileWrites;
        std::fprintf(stderr,"TEST tile write: buffer=%llx range=%llu\n",(unsigned long long)tileBinding.buffer,(unsigned long long)tileBinding.range);
    }
    originalUpdate(d,n,w,c,copies);
}
static void copy(VkCommandBuffer cmd,VkBuffer src,VkBuffer dst,uint32_t n,const VkBufferCopy *regions) {
    if(dst==watchedBuffer) { REQUIRE(n==1);tileCopy=*regions;++tileCopies; }
    originalCopy(cmd,src,dst,n,regions);
}
static void barrier(VkCommandBuffer cmd,VkPipelineStageFlags src,VkPipelineStageFlags dst,VkDependencyFlags flags,
    uint32_t nm,const VkMemoryBarrier *memory,uint32_t nb,const VkBufferMemoryBarrier *buffers,uint32_t ni,const VkImageMemoryBarrier *images) {
    for(uint32_t i=0;i<nb;++i) if(buffers[i].buffer==watchedBuffer) {
        if(dst & VK_PIPELINE_STAGE_TRANSFER_BIT) ++tileBefore;
        if(src & VK_PIPELINE_STAGE_TRANSFER_BIT) ++tileAfter;
    }
    originalBarrier(cmd,src,dst,flags,nm,memory,nb,buffers,ni,images);
}
static void prepare(RT64::FramebufferRenderer &renderer,RT64::RenderWorker &worker,RT64::DrawBuffers &buffers,RT64::OutputBuffers &outputs,uint64_t count) {
#if defined(TOOIE_GPU_TILES_BASELINE_CONTROL)
    renderer.endFramebuffers(&worker,&buffers,&outputs,false);
#else
    renderer.endFramebuffers(&worker,&buffers,&outputs,false,count);
#endif
}

int main(int argc,char **argv) {
    HostVulkanValidation validation;
    {
        VulkanDevice device(&validation.interface,"llvmpipe"); REQUIRE(device.vk);
        RT64::RenderWorker worker(&device,"tile-regression",RenderCommandListType::DIRECT);
        RT64::ShaderLibrary shaders(false,false);
        for(auto *s:{&shaders.samplerLibrary.linear,&shaders.samplerLibrary.nearest})
            for(auto *slot:{&s->wrapWrap,&s->wrapMirror,&s->wrapClamp,&s->mirrorWrap,&s->mirrorMirror,&s->mirrorClamp,&s->clampWrap,&s->clampMirror,&s->clampClamp})
                *slot=device.createSampler(RenderSamplerDesc());
        RT64::FramebufferRenderer renderer(&worker,false,RT64::UserConfiguration::GraphicsAPI::Vulkan,&shaders);
        RT64::DrawBuffers buffers; RT64::OutputBuffers outputs;
        RT64::BufferUploader drawUploader(&device), tileUploader(&device);
        // Exactly the common input allocation behavior of uploadDrawData for an
        // initially empty workload: the real uploader creates these pairs even
        // for empty vectors. It intentionally does NOT create the GPU tile pair.
        drawUploader.submit(&worker,{
            {nullptr,{0,0},sizeof(interop::RDPParams),RenderBufferFlag::STORAGE,{},&buffers.rdpParamsBuffer},
            {nullptr,{0,0},sizeof(interop::RDPTile),RenderBufferFlag::STORAGE,{},&buffers.rdpTilesBuffer},
            {nullptr,{0,0},sizeof(interop::RenderParams),RenderBufferFlag::STORAGE,{},&buffers.renderParamsBuffer}
        });
        drawUploader.wait();
        REQUIRE(!buffers.gpuTilesBuffer.get());
        // Deterministic otherwise-unused frame-parameter bytes in this fixture.
        std::memset(&renderer.frameParams,0,sizeof(renderer.frameParams));
        originalUpdate=vkUpdateDescriptorSets;vkUpdateDescriptorSets=update;
        originalCopy=vkCmdCopyBuffer;vkCmdCopyBuffer=copy;
        originalBarrier=vkCmdPipelineBarrier;vkCmdPipelineBarrier=barrier;
#if defined(TOOIE_GPU_TILES_BASELINE_CONTROL)
        prepare(renderer,worker,buffers,outputs,0); // Must reject old index 3/binding 4.
        REQUIRE(false);
#else
        static_assert(sizeof(interop::GPUTile)==52 && offsetof(interop::GPUTile,textureIndex)==32 && offsetof(interop::GPUTile,flags)==48);
        const std::string mode=argc>1?argv[1]:"normal";
        if(mode!="normal") {
            if(mode=="source-missing") buffers.gpuTilesBuffer.submit(&tileUploader,&worker,nullptr,{0,1});
            if(mode=="range-invalid") buffers.gpuTilesBuffer.submit(&tileUploader,&worker,nullptr,{1,0});
            if(mode=="size-overflow") buffers.gpuTilesBuffer.byteRange(UINT64_MAX);
            if(mode=="growth-overflow") buffers.gpuTilesBuffer.byteRange((UINT64_MAX/3)/sizeof(interop::GPUTile)+1);
            if(mode=="prefix-missing") {interop::GPUTile tile;buffers.gpuTilesBuffer.submit(&tileUploader,&worker,&tile,{1,2});}
            if(mode=="missing-empty" || mode=="missing-real") prepare(renderer,worker,buffers,outputs,mode=="missing-real"?1:0);
            buffers.gpuTilesBuffer.submit(&tileUploader,&worker,nullptr,{0,0});tileUploader.wait();
            if(mode=="stale") device.descriptorResources.erase(buffers.gpuTilesBuffer.get());
            if(mode=="range-small") buffers.gpuTilesBuffer.allocatedSize=1;
            prepare(renderer,worker,buffers,outputs,mode=="empty-as-real"?1:0);
            REQUIRE(false);
        }
        std::vector<interop::GPUTile> tiles;
        auto dst=device.createTexture(RenderTextureDesc::Texture2D(1,1,1,RenderFormat::R8G8B8A8_UNORM,RenderTextureFlag::STORAGE));
        RenderDescriptorSetBuilder result;result.begin();const uint32_t outIndex=result.addReadWriteTexture(0);result.end();
        auto out=result.create(&device);out->setTexture(outIndex,dst.get(),RenderTextureLayout::GENERAL);
        auto shader=device.createShader(GPUTilesTest,sizeof(GPUTilesTest),"main",RenderShaderFormat::SPIRV);
        std::unique_ptr<RenderPipelineLayout> layout;
        std::unique_ptr<RenderPipeline> pipeline;
        VkBuffer previousBuffer=VK_NULL_HANDLE;
        // Fresh empty -> real -> growth -> reused empty -> real -> partial
        // append -> fresh recreation. Prior work is fenced before replacement.
        unsigned step=0;
        for(uint32_t count:{0u,1u,64u,0u,2u,3u,0u,1u}) {
            const bool append=step==5,recreate=step==6;
            if(recreate) {buffers.gpuTilesBuffer.defaultBuffer.reset();buffers.gpuTilesBuffer.uploadBuffer.reset();buffers.gpuTilesBuffer.allocatedSize=0;}
            const size_t first=append?tiles.size():0;
            tiles.resize(count);
            for(size_t i=first;i<tiles.size();++i) {
                std::memset(&tiles[i],0,sizeof(tiles[i]));tiles[i].ulScale=interop::float2(1.0f,1.0f);tiles[i].flags.value=1;
            }
            buffers.gpuTilesBuffer.submit(&tileUploader,&worker,tiles.data(),{first,tiles.size()});
            REQUIRE(tiles.size()==count && buffers.gpuTilesBuffer.preparedTileCount==count);
            watchedBuffer=static_cast<const VulkanBuffer *>(buffers.gpuTilesBuffer.get())->vk;
            REQUIRE(static_cast<const VulkanBuffer *>(buffers.gpuTilesBuffer.get())->bufferUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            tileCopies=tileBefore=tileAfter=0;
            const unsigned writesBefore=tileWrites;
            prepare(renderer,worker,buffers,outputs,count);
            REQUIRE(tileWrites==writesBefore+1 && tileBinding.buffer==watchedBuffer && tileBinding.offset==0 && tileBinding.range==sizeof(interop::GPUTile)*std::max(count,1u));
            if(step==2 || recreate) REQUIRE(watchedBuffer!=previousBuffer);
            if(step==3 || step==4 || append) REQUIRE(watchedBuffer==previousBuffer);
            if(!pipeline) {
                RenderDescriptorSetDesc sets[]={renderer.descCommonSet->builder.descriptorSetDesc,renderer.descTextureSet->builder.descriptorSetDesc,result.descriptorSetDesc};
                RenderPushConstantRange push(0,0,0,8,RenderShaderStageFlag::COMPUTE);
                layout=device.createPipelineLayout(RenderPipelineLayoutDesc(&push,1,sets,3));
                RenderComputePipelineDesc desc;desc.pipelineLayout=layout.get();desc.computeShader=shader.get();pipeline=device.createComputePipeline(desc);
            }
            RenderColor expected(0.125f,0.25f,0.375f,0.625f);
            auto src=device.createTexture(RenderTextureDesc::ColorTarget(1,1,RenderFormat::R8G8B8A8_UNORM));
            const RenderTexture *color=src.get();auto fb=device.createFramebuffer(RenderFramebufferDesc(&color,1));
            if(count) {expected=RenderColor(0.25f,0.5f,0.75f,0.625f);renderer.descTextureSet->setTexture(0,src.get(),RenderTextureLayout::SHADER_READ);}
            worker.commandList->begin();
            // The same before/copy/after sequence and CPU wait used by
            // recordSetup/waitForUploaders, including the renderer's own uploads.
            for(auto *u:{&drawUploader,&tileUploader,renderer.shaderUploader.get()}) u->commandListBeforeBarriers(&worker);
            for(auto *u:{&drawUploader,&tileUploader,renderer.shaderUploader.get()}) u->commandListCopyResources(&worker);
            for(auto *u:{&drawUploader,&tileUploader,renderer.shaderUploader.get()}) u->commandListAfterBarriers(&worker);
            REQUIRE(tileCopies==1 && tileBefore && tileAfter);
            REQUIRE(tileCopy.srcOffset==first*sizeof(interop::GPUTile) && tileCopy.dstOffset==tileCopy.srcOffset && tileCopy.size==sizeof(interop::GPUTile)*(append?1:std::max(count,1u)));
            worker.commandList->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(src.get(),RenderTextureLayout::COLOR_WRITE));
            worker.commandList->setFramebuffer(fb.get());worker.commandList->clearColor(0,expected);worker.commandList->setFramebuffer(nullptr);
            RenderTextureBarrier barriers[]={RenderTextureBarrier(src.get(),RenderTextureLayout::SHADER_READ),RenderTextureBarrier(dst.get(),RenderTextureLayout::GENERAL)};
            worker.commandList->barriers(RenderBarrierStage::COMPUTE,nullptr,0,barriers,2);
            worker.commandList->setComputePipelineLayout(layout.get());worker.commandList->setPipeline(pipeline.get());
            worker.commandList->setComputeDescriptorSet(renderer.descCommonSet->get(),0);
            worker.commandList->setComputeDescriptorSet(renderer.descTextureSet->get(),1);worker.commandList->setComputeDescriptorSet(out.get(),2);
            const uint32_t constants[]={count,count?count-1:0};worker.commandList->setComputePushConstants(0,constants);worker.commandList->dispatch(1,1,1);
            auto readback=device.createBuffer(RenderBufferDesc::ReadbackBuffer(tileBinding.range));
            worker.commandList->barriers(RenderBarrierStage::COPY,RenderBufferBarrier(buffers.gpuTilesBuffer.defaultBuffer.get(),RenderBufferAccess::READ));
            worker.commandList->copyBufferRegion(readback->at(0),buffers.gpuTilesBuffer.get()->at(0),tileBinding.range);
            auto *rb=static_cast<VulkanBuffer *>(readback.get());auto *cmd=static_cast<VulkanCommandList *>(worker.commandList.get());
            VkBufferMemoryBarrier host={};host.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;host.buffer=rb->vk;host.size=VK_WHOLE_SIZE;
            host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            vkCmdPipelineBarrier(cmd->vk,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
            tileUploader.wait();renderer.waitForUploaders();
            auto pixels=readImage(device,worker.commandList.get(),worker.commandQueue.get(),worker.commandFence.get(),dst.get(),1,1,4);expectPixels(pixels,{expected},RenderFormat::R8G8B8A8_UNORM);
            REQUIRE(vmaInvalidateAllocation(device.allocator,rb->allocation,0,VK_WHOLE_SIZE)==VK_SUCCESS);
            const auto *bytes=static_cast<const uint8_t *>(readback->map());REQUIRE(bytes);
            if(count) REQUIRE(std::memcmp(bytes,tiles.data(),tileBinding.range)==0);
            else for(size_t i=0;i<tileBinding.range;++i) REQUIRE(bytes[i]==0);
            readback->unmap();previousBuffer=watchedBuffer;++step;
            std::printf("PASS: real framebuffer preparation logical=%u range=%llu current buffer/copy/barriers; initialized bytes and RGB/alpha consumed\n",count,(unsigned long long)tileBinding.range);
        }
        REQUIRE(tileWrites==8);
        vkUpdateDescriptorSets=originalUpdate;vkCmdCopyBuffer=originalCopy;vkCmdPipelineBarrier=originalBarrier;
#endif
    }
    REQUIRE(validationErrors==0);
}
