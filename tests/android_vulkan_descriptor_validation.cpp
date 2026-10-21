#include "android_vulkan_validation_common.h"
#include "descriptor-test.spirv.h"
#include "render/rt64_descriptor_sets.h"
#include <array>
static std::unique_ptr<RenderBuffer> upload(VulkanDevice &device,const void *data,size_t size,RenderBufferFlags flags) {
 auto b=device.createBuffer(RenderBufferDesc::UploadBuffer(256,flags));void *mapped=b->map();REQUIRE(mapped);std::memset(mapped,0,256);std::memcpy(mapped,data,size);b->unmap();return b;
}
int main() {
 HostVulkanValidation validation;
 {
  VulkanDevice d(&validation.interface,"llvmpipe");REQUIRE(d.vk);
  auto queue=d.createCommandQueue(RenderCommandListType::DIRECT);auto list=queue->createCommandList();auto fence=d.createCommandFence();
  RenderDescriptorSetBuilder buffers;buffers.begin();auto f=buffers.addFormattedBuffer(1),u=buffers.addFormattedBuffer(2),s=buffers.addFormattedBuffer(3),h=buffers.addFormattedBuffer(4);
  auto uniform=buffers.addConstantBuffer(5),structured=buffers.addStructuredBuffer(6),output=buffers.addReadWriteStructuredBuffer(7);buffers.end();
  RenderDescriptorSetBuilder images;images.begin();auto sampled=images.addTexture(1),sampler=images.addSampler(2),storage=images.addReadWriteTexture(3);images.end();
  RenderDescriptorSetDesc descriptions[]={buffers.descriptorSetDesc,images.descriptorSetDesc};
  auto layout=d.createPipelineLayout(RenderPipelineLayoutDesc(nullptr,0,descriptions,2));
  auto shader=d.createShader(DescriptorTest,sizeof(DescriptorTest),"main",RenderShaderFormat::SPIRV);
  RenderComputePipelineDesc pipelineDesc;pipelineDesc.pipelineLayout=layout.get();pipelineDesc.computeShader=shader.get();auto pipeline=d.createComputePipeline(pipelineDesc);
  std::array<float,4> floats={0.25f,0.5f,0.75f,1.0f};std::array<uint8_t,4> bytes={0,17,127,255};std::array<int8_t,4> signedBytes={-128,-1,0,127};std::array<uint16_t,4> shorts={0,255,32768,65535};
  std::array<uint32_t,4> params={7,0,0,0};std::array<uint32_t,64> input={};
  const uint64_t alignment=std::max<uint64_t>(16,d.physicalDeviceProperties.limits.minStorageBufferOffsetAlignment);
  REQUIRE(alignment+4<=256);input[alignment/4]=11;
  auto bf=upload(d,floats.data(),sizeof(floats),RenderBufferFlag::FORMATTED);auto bu=upload(d,bytes.data(),sizeof(bytes),RenderBufferFlag::FORMATTED);
  auto bs=upload(d,signedBytes.data(),sizeof(signedBytes),RenderBufferFlag::FORMATTED|RenderBufferFlag::STORAGE);auto bh=upload(d,shorts.data(),sizeof(shorts),RenderBufferFlag::FORMATTED);
  auto bc=upload(d,params.data(),sizeof(params),RenderBufferFlag::CONSTANT);auto bi=upload(d,input.data(),sizeof(input),RenderBufferFlag::STORAGE);
  const auto signedUsage=static_cast<VulkanBuffer *>(bs.get())->bufferUsage;
  REQUIRE((signedUsage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) && (signedUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) && !(signedUsage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT));
  auto vf=bf->createBufferFormattedView(RenderFormat::R32_FLOAT);auto vu=bu->createBufferFormattedView(RenderFormat::R8_UINT);
  auto vs=bs->createBufferFormattedView(RenderFormat::R8_SINT);auto vh=bh->createBufferFormattedView(RenderFormat::R16_UINT);
  auto out=d.createBuffer(RenderBufferDesc::DefaultBuffer(64,RenderBufferFlag::STORAGE|RenderBufferFlag::UNORDERED_ACCESS));auto readback=d.createBuffer(RenderBufferDesc::ReadbackBuffer(64));
  auto writableTyped=d.createBuffer(RenderBufferDesc::DefaultBuffer(256,RenderBufferFlag::STORAGE|RenderBufferFlag::FORMATTED|RenderBufferFlag::UNORDERED_ACCESS));
  REQUIRE(static_cast<VulkanBuffer *>(writableTyped.get())->bufferUsage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
  auto writableView=writableTyped->createBufferFormattedView(RenderFormat::R32_UINT);
  RenderDescriptorSetBuilder writableBuilder;writableBuilder.begin();auto writableIndex=writableBuilder.addReadWriteFormattedBuffer(1);writableBuilder.end();
  auto writableSet=writableBuilder.create(&d);writableSet->setBuffer(writableIndex,writableTyped.get(),0,nullptr,writableView.get());
  auto sample=d.createSampler(RenderSamplerDesc());
  auto set=buffers.create(&d);set->setBuffer(f,bf.get(),0,nullptr,vf.get());set->setBuffer(u,bu.get(),0,nullptr,vu.get());set->setBuffer(s,bs.get(),0,nullptr,vs.get());set->setBuffer(h,bh.get(),0,nullptr,vh.get());
  set->setBuffer(uniform,bc.get(),16);RenderBufferStructuredView view(4,uint32_t(alignment/4));set->setBuffer(structured,bi.get(),4,&view);set->setBuffer(output,out.get(),64);
  for(unsigned cycle=0;cycle<3;++cycle) {
   auto src=d.createTexture(RenderTextureDesc::ColorTarget(4,1,RenderFormat::R8G8B8A8_UNORM));auto dst=d.createTexture(RenderTextureDesc::Texture2D(4,1,1,RenderFormat::R8G8B8A8_UNORM,RenderTextureFlag::STORAGE));
   const RenderTexture *color=src.get();auto fb=d.createFramebuffer(RenderFramebufferDesc(&color,1));
   auto imageSet=images.create(&d);auto explicitView=src->createTextureView(RenderTextureViewDesc::Texture2D(RenderFormat::R8G8B8A8_UNORM));
   imageSet->setTexture(sampled,src.get(),RenderTextureLayout::SHADER_READ,explicitView.get());imageSet->setSampler(sampler,sample.get());imageSet->setTexture(storage,dst.get(),RenderTextureLayout::GENERAL);
   RenderColor expected(0.25f,0.5f,0.75f,0.25f*(cycle+1));list->begin();list->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(src.get(),RenderTextureLayout::COLOR_WRITE));list->setFramebuffer(fb.get());list->clearColor(0,expected);
   list->setFramebuffer(nullptr);RenderTextureBarrier tb[]={RenderTextureBarrier(src.get(),RenderTextureLayout::SHADER_READ),RenderTextureBarrier(dst.get(),RenderTextureLayout::GENERAL)};
   list->barriers(RenderBarrierStage::COMPUTE,nullptr,0,tb,2);list->barriers(RenderBarrierStage::COMPUTE,RenderBufferBarrier(out.get(),RenderBufferAccess::WRITE));
   list->setPipeline(pipeline.get());list->setComputePipelineLayout(layout.get());list->setComputeDescriptorSet(set.get(),0);list->setComputeDescriptorSet(imageSet.get(),1);list->dispatch(4,1,1);
   list->barriers(RenderBarrierStage::COPY,RenderBufferBarrier(out.get(),RenderBufferAccess::READ));list->copyBuffer(readback.get(),out.get());
   auto pixels=readImage(d,list.get(),queue.get(),fence.get(),dst.get(),4,1,4);expectPixels(pixels,std::vector<RenderColor>(4,expected),RenderFormat::R8G8B8A8_UNORM);
   auto *vb=static_cast<VulkanBuffer *>(readback.get());REQUIRE(vmaInvalidateAllocation(d.allocator,vb->allocation,0,VK_WHOLE_SIZE)==VK_SUCCESS);
   const auto *words=static_cast<const uint32_t *>(readback->map());
   for(unsigned i=0;i<4;++i) {uint32_t bits;std::memcpy(&bits,&floats[i],4);REQUIRE(words[4*i]==bits && words[4*i+1]==unsigned(bytes[i])+7 && words[4*i+2]==unsigned(int(signedBytes[i])+128) && words[4*i+3]==unsigned(shorts[i])+11);}
   readback->unmap();
  }
  std::puts("PASS: real Vulkan typed R32_FLOAT/R8_UINT/R8_SINT/R16_UINT, uniform/structured offset, sampled/storage image and sampler descriptors consumed; RGB/alpha and buffer readbacks; 3 resource recreation cycles");
 }
 REQUIRE(validationErrors==0);std::puts("PASS: descriptor Vulkan core/synchronization validation: zero errors");
}
