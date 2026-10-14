// Production descriptor setters and RT64 descriptor layouts, with a driver
// recorder that must never see an unsafe update. No ROM or GPU is needed.
#include "plume_vulkan.h"
#include "render/rt64_rsp_processor.h"
#include "render/rt64_vertex_processor.h"
#include "../src/android_descriptor_context.h"
#include "plume_vulkan_descriptor_probe.h"
#include <atomic>
#include <future>
#include <cstdio>
#include <cstring>
using namespace plume;
#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::abort(); } } while(0)
extern "C" void ANativeWindow_acquire(ANativeWindow *) {}
extern "C" void ANativeWindow_release(ANativeWindow *) {}
extern "C" int32_t ANativeWindow_getWidth(ANativeWindow *) { return 1; }
extern "C" int32_t ANativeWindow_getHeight(ANativeWindow *) { return 1; }
template<class T> T handle() { static uintptr_t n=1; return reinterpret_cast<T>(n++); }
static unsigned updates=0;
static bool prohibitUpdate=false, failLayout=false, failPool=false, failAllocate=false, nullAllocate=false, failView=false, failImageView=false;
struct Layout { std::vector<VkDescriptorSetLayoutBinding> bindings; std::vector<VkDescriptorBindingFlags> flags; VkDescriptorSetLayoutCreateFlags creationFlags = 0; };
struct Pool { std::unordered_map<VkDescriptorType,uint32_t> remaining; uint32_t sets; VkDescriptorPoolCreateFlags flags; };
static std::unordered_map<VkDescriptorSetLayout,Layout> layouts;
static std::unordered_map<VkDescriptorPool,Pool> pools;
static std::unordered_map<VkDescriptorSet,std::pair<VkDescriptorSetLayout,uint32_t>> sets;
static bool layoutSupported=true, shortPool=false;
static uint32_t variableLimit=8192;
static void layoutSupport(VkDevice,const VkDescriptorSetLayoutCreateInfo *,VkDescriptorSetLayoutSupport *out) {
 out->supported=layoutSupported;
 if(out->pNext) static_cast<VkDescriptorSetVariableDescriptorCountLayoutSupport *>(out->pNext)->maxVariableDescriptorCount=variableLimit;
}
static bool storageTexelSupported=true;
static void formatProperties(VkPhysicalDevice,VkFormat,VkFormatProperties *out) { *out={};out->bufferFeatures=VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT | (storageTexelSupported?VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT:0); }
static VkWriteDescriptorSet lastWrite={};
static VkDescriptorBufferInfo lastBuffer={};
static std::promise<void> *entered=nullptr;
static std::shared_future<void> updateRelease;
static VkResult enumerate(VkInstance,uint32_t *n,VkPhysicalDevice *) { *n=0; return VK_SUCCESS; }
static VkResult createLayout(VkDevice,const VkDescriptorSetLayoutCreateInfo *info,const VkAllocationCallbacks *,VkDescriptorSetLayout *out) {
 *out=failLayout? VK_NULL_HANDLE:handle<VkDescriptorSetLayout>();
 if(!failLayout) {
  auto &l=layouts[*out];l.bindings.assign(info->pBindings,info->pBindings+info->bindingCount);l.creationFlags=info->flags;l.flags.resize(info->bindingCount,0);
  if(info->pNext) {const auto *flags=static_cast<const VkDescriptorSetLayoutBindingFlagsCreateInfo *>(info->pNext);REQUIRE(flags->sType==VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO && flags->bindingCount==info->bindingCount);l.flags.assign(flags->pBindingFlags,flags->pBindingFlags+flags->bindingCount);}
 }
 return failLayout?VK_ERROR_OUT_OF_HOST_MEMORY:VK_SUCCESS;
}
static void destroyLayout(VkDevice,VkDescriptorSetLayout v,const VkAllocationCallbacks *) { layouts.erase(v); }
static VkResult createPool(VkDevice,const VkDescriptorPoolCreateInfo *info,const VkAllocationCallbacks *,VkDescriptorPool *out) {
 *out=failPool?VK_NULL_HANDLE:handle<VkDescriptorPool>();
 REQUIRE(info->maxSets==1);
 if(*out) {auto &p=pools[*out];p.sets=info->maxSets;p.flags=info->flags;
  for(uint32_t i=0;i<info->poolSizeCount;++i) {auto s=info->pPoolSizes[i];REQUIRE(s.descriptorCount && !p.remaining.count(s.type));p.remaining[s.type]=shortPool?0:s.descriptorCount;}
 }
 return failPool?VK_ERROR_OUT_OF_HOST_MEMORY:VK_SUCCESS;
}
static void destroyPool(VkDevice,VkDescriptorPool p,const VkAllocationCallbacks *) {pools.erase(p);}
static VkResult allocate(VkDevice,const VkDescriptorSetAllocateInfo *info,VkDescriptorSet *out) {
 REQUIRE(info->descriptorSetCount==1);
 auto &pool=pools.at(info->descriptorPool);auto &layout=layouts.at(*info->pSetLayouts);
 auto remaining=pool.remaining;
 if(layout.creationFlags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT) REQUIRE(pool.flags & VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
 for(size_t i=0;i<layout.bindings.size();++i) {
  auto &b=layout.bindings[i];uint32_t count=b.descriptorCount;
  if(layout.flags[i] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) {
   const auto *v=static_cast<const VkDescriptorSetVariableDescriptorCountAllocateInfo *>(info->pNext);
   REQUIRE(v && v->sType==VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO && v->descriptorSetCount==1 && v->pDescriptorCounts);
   count=v->pDescriptorCounts[0];REQUIRE(count<=b.descriptorCount);
  }
  // Immutable samplers still consume sampler descriptors in the pool.
  if(remaining[b.descriptorType]<count || !pool.sets) {*out=VK_NULL_HANDLE;return VK_ERROR_OUT_OF_POOL_MEMORY;}
  remaining[b.descriptorType]-=count;
 }
 if(failAllocate) {*out=handle<VkDescriptorSet>();return VK_ERROR_OUT_OF_POOL_MEMORY;} // Failed output is not usable.
 *out=nullAllocate?VK_NULL_HANDLE:handle<VkDescriptorSet>();
 if(*out) { auto count=info->pNext?static_cast<const VkDescriptorSetVariableDescriptorCountAllocateInfo *>(info->pNext)->pDescriptorCounts[0]:UINT32_MAX;
 sets[*out]={*info->pSetLayouts,count};pool.remaining=std::move(remaining);--pool.sets; }
 return VK_SUCCESS;
}
static void update(VkDevice,uint32_t n,const VkWriteDescriptorSet *w,uint32_t copies,const VkCopyDescriptorSet *) {
 REQUIRE(!prohibitUpdate && n==1 && copies==0 && w->dstSet && w->descriptorCount==1);
 auto set=sets.at(w->dstSet); const VkDescriptorSetLayoutBinding *binding=nullptr;
 for(const auto &b:layouts.at(set.first).bindings) if(b.binding==w->dstBinding) binding=&b;
 REQUIRE(binding && binding->descriptorType==w->descriptorType && w->dstArrayElement < binding->descriptorCount);
 if(set.second!=UINT32_MAX && w->dstBinding==layouts.at(set.first).bindings.back().binding) REQUIRE(w->dstArrayElement<set.second);
 switch(w->descriptorType) {
 case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
  REQUIRE(w->pTexelBufferView && *w->pTexelBufferView && !w->pBufferInfo && !w->pImageInfo); break;
 case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
  REQUIRE(w->pBufferInfo && w->pBufferInfo->buffer && w->pBufferInfo->range && !w->pTexelBufferView && !w->pImageInfo); lastBuffer=*w->pBufferInfo; break;
 case VK_DESCRIPTOR_TYPE_SAMPLER: REQUIRE(w->pImageInfo && w->pImageInfo->sampler && !w->pImageInfo->imageView); break;
 case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: REQUIRE(w->pImageInfo && w->pImageInfo->imageView && !w->pImageInfo->sampler); break;
 default: REQUIRE(false);
 }
 lastWrite=*w; ++updates;
 if(entered) { auto *p=entered; entered=nullptr; p->set_value(); updateRelease.wait(); }
}
static VkResult createBufferView(VkDevice,const VkBufferViewCreateInfo *info,const VkAllocationCallbacks *,VkBufferView *out) {
 REQUIRE(info->buffer); *out=failView?VK_NULL_HANDLE:handle<VkBufferView>(); return failView?VK_ERROR_FORMAT_NOT_SUPPORTED:VK_SUCCESS;
}
static void destroyBufferView(VkDevice,VkBufferView,const VkAllocationCallbacks *) {}
static VkResult createImageView(VkDevice,const VkImageViewCreateInfo *info,const VkAllocationCallbacks *,VkImageView *out) { REQUIRE(info->image); *out=handle<VkImageView>(); return failImageView?VK_ERROR_FORMAT_NOT_SUPPORTED:VK_SUCCESS; }
static void destroyImageView(VkDevice,VkImageView,const VkAllocationCallbacks *) {}
static VkResult createSampler(VkDevice,const VkSamplerCreateInfo *,const VkAllocationCallbacks *,VkSampler *out) { *out=handle<VkSampler>(); return VK_SUCCESS; }
static void destroySampler(VkDevice,VkSampler,const VkAllocationCallbacks *) {}
struct Buffer : VulkanBuffer {
 explicit Buffer(VulkanDevice &d,uint64_t size=256,RenderBufferFlags flags=RenderBufferFlag::STORAGE|RenderBufferFlag::FORMATTED|RenderBufferFlag::CONSTANT) {
  device=&d; vk=handle<VkBuffer>();bufferUsage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT; desc=RenderBufferDesc::DefaultBuffer(size,flags); d.descriptorResources.track(this,VK_OBJECT_TYPE_BUFFER,uint64_t(vk));
 }
 ~Buffer() { vk=VK_NULL_HANDLE; } // Synthetic host handle, no VMA allocation.
};
static std::unique_ptr<VulkanTexture> image(VulkanDevice &d) {
 auto t=std::make_unique<VulkanTexture>(&d,handle<VkImage>());
 t->desc=RenderTextureDesc::Texture2D(16,16,1,RenderFormat::R8G8B8A8_UNORM,RenderTextureFlag::STORAGE);
 t->imageFormat=VK_FORMAT_R8G8B8A8_UNORM; t->imageUsage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
 t->fillSubresourceRange(); t->createImageView(t->imageFormat); return t;
}
static std::unique_ptr<RenderDescriptorSet> makeSet(VulkanDevice &d,RenderDescriptorRangeType type,uint32_t count=1,uint32_t actual=0) {
 RenderDescriptorRange range(type,7,count,nullptr); RenderDescriptorSetDesc desc; desc.descriptorRanges=&range;desc.descriptorRangesCount=1;
 desc.lastRangeIsBoundless=actual!=0;desc.boundlessRangeSize=actual;return d.createDescriptorSet(desc);
}
int main(int argc,char **argv) {
 VulkanInterface interface; vkEnumeratePhysicalDevices=enumerate; VulkanDevice device(&interface,"");
 device.vk=handle<VkDevice>();
 device.descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount=VK_TRUE;
 device.descriptorIndexingFeatures.descriptorBindingPartiallyBound=VK_TRUE;
 device.descriptorIndexingFeatures.runtimeDescriptorArray=VK_TRUE;
 device.descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind=VK_TRUE;
 device.descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages=65536;
 device.descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSampledImages=65536;
 device.descriptorIndexingProperties.maxUpdateAfterBindDescriptorsInAllPools=65536;
 vkGetDescriptorSetLayoutSupport=layoutSupport;
 vkGetPhysicalDeviceFormatProperties=formatProperties;
 vkCreateDescriptorSetLayout=createLayout;vkDestroyDescriptorSetLayout=destroyLayout;vkCreateDescriptorPool=createPool;vkDestroyDescriptorPool=destroyPool;
 vkAllocateDescriptorSets=allocate;vkUpdateDescriptorSets=update;vkCreateBufferView=createBufferView;vkDestroyBufferView=destroyBufferView;
 vkCreateImageView=createImageView;vkDestroyImageView=destroyImageView;vkCreateSampler=createSampler;vkDestroySampler=destroySampler;
 if(argc==2 && std::strcmp(argv[1],"probe-exhaustion-control")==0) {
  shortPool=true;prohibitUpdate=true;
  REQUIRE(!runVulkanDescriptorAllocationProbe(device.vk,device.descriptorIndexingFeatures));
  REQUIRE(pools.empty() && layouts.empty());device.vk=VK_NULL_HANDLE;
  std::puts("PASS: probe preserves exhausted-pool failure and retires all five independent cases");return 0;
 }
 if(argc>1) {
  prohibitUpdate=true;const std::string reason=argv[1];
  failLayout=reason=="layout-failed";failPool=reason=="pool-failed";failAllocate=reason=="allocation-failed";nullAllocate=reason=="allocation-null";
  if(reason=="invalid-range") {makeSet(device,RenderDescriptorRangeType::UNKNOWN);REQUIRE(false);}
  if(reason=="variable-too-large") {makeSet(device,RenderDescriptorRangeType::TEXTURE,2,3);REQUIRE(false);}
  if(reason=="layout-empty-variable") {RenderDescriptorSetDesc empty;empty.lastRangeIsBoundless=true;VulkanDescriptorSetLayout invalid(&device,empty);REQUIRE(false);}
  if(reason=="layout-null-ranges") {RenderDescriptorSetDesc invalid;invalid.descriptorRangesCount=1;VulkanDescriptorSetLayout bad(&device,invalid);REQUIRE(false);}
  if(reason=="layout-invalid-type") {RenderDescriptorRange bad(RenderDescriptorRangeType::UNKNOWN,0,1,nullptr);VulkanDescriptorSetLayout invalid(&device,RenderDescriptorSetDesc(&bad,1));REQUIRE(false);}
  if(reason=="layout-duplicate-binding") {RenderDescriptorRange ranges[]={RenderDescriptorRange(RenderDescriptorRangeType::SAMPLER,0,1,nullptr),RenderDescriptorRange(RenderDescriptorRangeType::TEXTURE,0,1,nullptr)};VulkanDescriptorSetLayout invalid(&device,RenderDescriptorSetDesc(ranges,2));REQUIRE(false);}
  if(reason=="layout-unsupported") {layoutSupported=false;makeSet(device,RenderDescriptorRangeType::TEXTURE);REQUIRE(false);}
  if(reason=="indexing-disabled") {device.descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount=VK_FALSE;makeSet(device,RenderDescriptorRangeType::TEXTURE,8192,1);REQUIRE(false);}
  if(reason=="uab-disabled") {device.descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind=VK_FALSE;makeSet(device,RenderDescriptorRangeType::TEXTURE,8192,1);REQUIRE(false);}
  if(reason=="stage-limit") {device.descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSampledImages=4096;makeSet(device,RenderDescriptorRangeType::TEXTURE,8192,1);REQUIRE(false);}
  if(reason=="variable-limit") {variableLimit=4096;makeSet(device,RenderDescriptorRangeType::TEXTURE,8192,1);REQUIRE(false);}
  if(reason=="variable-not-highest") {RenderDescriptorRange ranges[]={RenderDescriptorRange(RenderDescriptorRangeType::SAMPLER,9,1,nullptr),RenderDescriptorRange(RenderDescriptorRangeType::TEXTURE,0,8,nullptr)};device.createDescriptorSet(RenderDescriptorSetDesc(ranges,2,true,1));REQUIRE(false);}
  if(reason=="pool-capacity-control") {shortPool=true;makeSet(device,RenderDescriptorRangeType::TEXTURE,8192,1);REQUIRE(false);}
  auto set=makeSet(device,RenderDescriptorRangeType::FORMATTED_BUFFER);
  Buffer buffer(device);auto view=buffer.createBufferFormattedView(RenderFormat::R8_UINT);
  if(reason=="unsupported-view-format") {storageTexelSupported=false;view=buffer.createBufferFormattedView(RenderFormat::R8_SINT);set->setBuffer(0,&buffer,0,nullptr,view.get());}
  if(reason=="texel-element-limit") {device.physicalDeviceProperties.limits.maxTexelBufferElements=16;view=buffer.createBufferFormattedView(RenderFormat::R8_UINT);set->setBuffer(0,&buffer,0,nullptr,view.get());}
  if(reason=="missing-texel-view") set->setBuffer(0,&buffer);
  if(reason=="null-buffer") set->setBuffer(0,nullptr);
  if(reason=="stale-buffer") {device.descriptorResources.erase(&buffer);set->setBuffer(0,&buffer,0,nullptr,view.get());}
  if(reason=="stale-buffer-view") {const auto *p=view.get();view.reset();set->setBuffer(0,&buffer,0,nullptr,p);}
  if(reason=="failed-buffer-view") {failView=true;view=buffer.createBufferFormattedView(RenderFormat::R8_SINT);set->setBuffer(0,&buffer,0,nullptr,view.get());}
  if(reason=="wrong-buffer-parent") {Buffer other(device);set->setBuffer(0,&other,0,nullptr,view.get());}
  if(reason=="index-overflow") set->setBuffer(1,&buffer,0,nullptr,view.get());
  if(reason=="null-set") {static_cast<VulkanDescriptorSet *>(set.get())->vk=VK_NULL_HANDLE;set->setBuffer(0,&buffer);}
  if(reason=="set-handle-corrupted") {static_cast<VulkanDescriptorSet *>(set.get())->vk=handle<VkDescriptorSet>();set->setBuffer(0,&buffer);}
  if(reason=="texel-as-storage") {set=makeSet(device,RenderDescriptorRangeType::STRUCTURED_BUFFER);set->setBuffer(0,&buffer,0,nullptr,view.get());}
  if(reason=="buffer-usage") {set=makeSet(device,RenderDescriptorRangeType::STRUCTURED_BUFFER);buffer.bufferUsage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;set->setBuffer(0,&buffer,256);}
  if(reason=="range-overflow"||reason=="offset-overflow"||reason=="misaligned"||reason=="range-limit") {
   set=makeSet(device,RenderDescriptorRangeType::STRUCTURED_BUFFER);
   if(reason=="range-overflow") set->setBuffer(0,&buffer,257);
   if(reason=="offset-overflow") {RenderBufferStructuredView structured(65536,65536);set->setBuffer(0,&buffer,0,&structured);}
   if(reason=="misaligned") {device.physicalDeviceProperties.limits.minStorageBufferOffsetAlignment=16;RenderBufferStructuredView structured(4,1);set->setBuffer(0,&buffer,0,&structured);}
   if(reason=="range-limit") {device.physicalDeviceProperties.limits.maxStorageBufferRange=128;set->setBuffer(0,&buffer,VK_WHOLE_SIZE);}
  }
  auto texture=image(device);auto sampled=makeSet(device,RenderDescriptorRangeType::TEXTURE);
  if(reason=="failed-image-view") {failImageView=true;texture->createImageView(texture->imageFormat);sampled->setTexture(0,texture.get(),RenderTextureLayout::SHADER_READ);}
  if(reason=="null-image-view") {texture->imageView=VK_NULL_HANDLE;sampled->setTexture(0,texture.get(),RenderTextureLayout::SHADER_READ);}
  if(reason=="stale-image") {const auto *p=texture.get();texture.reset();sampled->setTexture(0,p,RenderTextureLayout::SHADER_READ);}
  if(reason=="poison-image") sampled->setTexture(0,reinterpret_cast<const RenderTexture *>(uintptr_t(0xdeadbeef)),RenderTextureLayout::SHADER_READ);
  if(reason=="stale-image-view") {auto v=texture->createTextureView(RenderTextureViewDesc::Texture2D(texture->desc.format));const auto *p=v.get();v.reset();sampled->setTexture(0,texture.get(),RenderTextureLayout::SHADER_READ,p);}
  if(reason=="wrong-image-parent") {auto other=image(device);auto v=other->createTextureView(RenderTextureViewDesc::Texture2D(other->desc.format));sampled->setTexture(0,texture.get(),RenderTextureLayout::SHADER_READ,v.get());}
  if(reason=="image-as-buffer") set->setTexture(0,texture.get(),RenderTextureLayout::SHADER_READ);
  if(reason=="image-usage") {texture->imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;sampled->setTexture(0,texture.get(),RenderTextureLayout::SHADER_READ);}
  if(reason=="storage-layout") {auto storage=makeSet(device,RenderDescriptorRangeType::READ_WRITE_TEXTURE);storage->setTexture(0,texture.get(),RenderTextureLayout::SHADER_READ);}
  if(reason=="variable-outside") {sampled=makeSet(device,RenderDescriptorRangeType::TEXTURE,8,2);sampled->setTexture(2,texture.get(),RenderTextureLayout::SHADER_READ);}
  if(reason=="image-parent-reused") {
   alignas(VulkanTexture) unsigned char memory[sizeof(VulkanTexture)];
   auto *parent=new(memory) VulkanTexture(&device,handle<VkImage>());
   parent->desc=texture->desc;parent->imageFormat=texture->imageFormat;parent->imageUsage=texture->imageUsage;parent->fillSubresourceRange();parent->createImageView(parent->imageFormat);
   auto old=parent->createTextureView(RenderTextureViewDesc::Texture2D(parent->desc.format));parent->~VulkanTexture();
   parent=new(memory) VulkanTexture(&device,handle<VkImage>());
   parent->desc=texture->desc;parent->imageFormat=texture->imageFormat;parent->imageUsage=texture->imageUsage;parent->fillSubresourceRange();parent->createImageView(parent->imageFormat);
   sampled->setTexture(0,parent,RenderTextureLayout::SHADER_READ,old.get());
  }
  auto sampler=device.createSampler(RenderSamplerDesc());auto samplerSet=makeSet(device,RenderDescriptorRangeType::SAMPLER);
  if(reason=="immutable-sampler-destroyed") {
   RenderDescriptorSetBuilder b;b.begin();b.addImmutableSampler(0,sampler.get());auto index=b.addStructuredBuffer(1);b.end();auto immutable=b.create(&device);sampler.reset();immutable->setBuffer(index,&buffer,256);
  }
  if(reason=="stale-sampler") {const auto *p=sampler.get();sampler.reset();samplerSet->setSampler(0,p);}
  if(reason=="sampler-as-image") sampled->setSampler(0,sampler.get());
  if(reason=="wrong-device") {device.descriptorResources.erase(sampler.get());samplerSet->setSampler(0,sampler.get());}
  REQUIRE(false);
 }
 {
  REQUIRE(runVulkanDescriptorAllocationProbe(device.vk,device.descriptorIndexingFeatures));
  REQUIRE(pools.empty() && layouts.empty());
  // A format that supports only uniform texel access must remain usable for
  // read-only formatted views, while writable views are rejected below.
  storageTexelSupported=false;Buffer readOnly(device);readOnly.bufferUsage=VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  auto readonlyView=readOnly.createBufferFormattedView(RenderFormat::R8_SINT);auto readonlySet=makeSet(device,RenderDescriptorRangeType::FORMATTED_BUFFER);readonlySet->setBuffer(0,&readOnly,0,nullptr,readonlyView.get());
  storageTexelSupported=true;
  Buffer b(device,512);auto view=b.createBufferFormattedView(RenderFormat::R8_UINT);
  auto typed=makeSet(device,RenderDescriptorRangeType::FORMATTED_BUFFER);typed->setBuffer(0,&b,0,nullptr,view.get());
  auto writable=makeSet(device,RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER);writable->setBuffer(0,&b,0,nullptr,view.get());
  auto storage=makeSet(device,RenderDescriptorRangeType::STRUCTURED_BUFFER);RenderBufferStructuredView sub(16,4);storage->setBuffer(0,&b,0,&sub);
  REQUIRE(lastBuffer.offset==64 && lastBuffer.range==448);
  Buffer big(device,1ull<<33);RenderBufferStructuredView large(65536,65536);storage->setBuffer(0,&big,16,&large);REQUIRE(lastBuffer.offset==1ull<<32);
  auto texture=image(device);auto tSet=makeSet(device,RenderDescriptorRangeType::TEXTURE,8,2);tSet->setTexture(1,texture.get(),RenderTextureLayout::SHADER_READ);
  auto explicitView=texture->createTextureView(RenderTextureViewDesc::Texture2D(texture->desc.format));tSet->setTexture(0,texture.get(),RenderTextureLayout::SHADER_READ,explicitView.get());
  auto sampler=device.createSampler(RenderSamplerDesc());auto samplerSet=makeSet(device,RenderDescriptorRangeType::SAMPLER);samplerSet->setSampler(0,sampler.get());
  // Exercise the actual framebuffer texture layout with small variable counts,
  // and mixed fixed/variable ranges sharing a type. No unused capacity remains.
  for(uint32_t n:{1u,2u,32u,8192u}) {
   RT64::FramebufferRendererDescriptorTextureSet fb(&device,n);
   auto *vkSet=static_cast<VulkanDescriptorSet *>(fb.get());auto &pool=pools.at(vkSet->descriptorPool);
   REQUIRE(pool.remaining[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE]==0 && pool.sets==0 && vkSet->allocatedCounts[0]==n);
  }
  RenderDescriptorSetBuilder mixed;mixed.begin();mixed.addImmutableSampler(0,sampler.get());mixed.addTexture(1,2);mixed.addTexture(2,8192);mixed.end(true,3);
  auto mixedSet=mixed.create(&device);auto &mixedPool=pools.at(static_cast<VulkanDescriptorSet *>(mixedSet.get())->descriptorPool);
  REQUIRE(mixedPool.remaining[VK_DESCRIPTOR_TYPE_SAMPLER]==0 && mixedPool.remaining[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE]==0);
  // The actual RSP layouts include the gameplay R32_FLOAT/R8_SINT/R16_UINT
  // typed inputs. A fill-only workload must not populate absent output buffers.
  RT64::RSPProcessor processor(&device);RT64::DrawData draw;RT64::DrawBuffers inputs;RT64::OutputBuffers outputs;
  RT64::RSPProcessor::ProcessParams p; p.drawData=&draw;p.drawBuffers=&inputs;p.outputBuffers=&outputs;
  const unsigned before=updates;processor.process(p);REQUIRE(updates==before && processor.processCB.vertexCount==0 && processor.modifyCB.modifyCount==0);
  RT64::VertexProcessor world(&device);RT64::VertexProcessor::ProcessParams wp;wp.drawData=&draw;wp.drawBuffers=&inputs;wp.outputBuffers=&outputs;
  world.process(wp);REQUIRE(updates==before && world.worldCB.vertexCount==0);
  auto populate=[&](RT64::BufferPair &pair,std::initializer_list<RenderFormat> formats) {
   pair.defaultBuffer=std::make_unique<Buffer>(device,512);pair.allocatedSize=512;
   for(auto format:formats) pair.defaultViews.emplace_back(pair.defaultBuffer->createBufferFormattedView(format));
  };
  for(auto *pair:{&inputs.positionBuffer,&inputs.velocityBuffer,&inputs.texcoordBuffer,&inputs.texcoordVelocityBuffer}) populate(*pair,{RenderFormat::R32_FLOAT});
  populate(inputs.normalColorBuffer,{RenderFormat::R8_UINT,RenderFormat::R8_SINT});
  for(auto *pair:{&inputs.viewProjIndicesBuffer,&inputs.worldIndicesBuffer,&inputs.fogIndicesBuffer,&inputs.lightIndicesBuffer,&inputs.lookAtIndicesBuffer}) populate(*pair,{RenderFormat::R16_UINT});
  populate(inputs.lightCountsBuffer,{RenderFormat::R8_UINT});populate(inputs.modifyPosUintsBuffer,{RenderFormat::R32_UINT});
  for(auto *pair:{&inputs.rspViewportsBuffer,&inputs.rspFogBuffer,&inputs.rspLightsBuffer,&inputs.rspLookAtBuffer,&inputs.viewProjTransformsBuffer,&inputs.worldTransformsBuffer,&inputs.invTWorldTransformsBuffer,&inputs.prevWorldTransformsBuffer}) populate(*pair,{});
  for(auto *computed:{&outputs.screenPosBuffer,&outputs.genTexCoordBuffer,&outputs.shadedColBuffer,&outputs.worldPosBuffer,&outputs.worldNormBuffer,&outputs.worldVelBuffer}) {computed->buffer=std::make_unique<Buffer>(device,512);computed->allocatedSize=512;}
  draw.posFloats.resize(12);draw.worldIndices.resize(3);const auto active=updates;processor.process(p);
  REQUIRE(processor.processCB.vertexCount==3 && updates==active+21);
  processor.process(p);REQUIRE(processor.processCB.vertexCount==0 && updates==active+21);
  world.process(wp);REQUIRE(world.worldCB.vertexCount==3 && updates==active+31);
  world.process(wp);REQUIRE(world.worldCB.vertexCount==0 && updates==active+31);
  draw.modifyPosUints.resize(2);processor.process(p);
  REQUIRE(processor.processCB.vertexCount==0 && processor.modifyCB.modifyCount==1 && updates==active+33);
  // Destruction waits for vkUpdateDescriptorSets to return. Never destroy a
  // sampler in the narrow interval after validation but before the driver copy.
  std::promise<void> enter,release; entered=&enter;updateRelease=release.get_future().share();auto *sample=sampler.get();
  auto writer=std::async(std::launch::async,[&]{samplerSet->setSampler(0,sample);});enter.get_future().wait();
  auto destroy=std::async(std::launch::async,[&]{sampler.reset();});REQUIRE(destroy.wait_for(std::chrono::milliseconds(30))==std::future_status::timeout);
  release.set_value();writer.get();destroy.get();
 }
 REQUIRE(device.descriptorResources.objects.empty() && pools.empty() && layouts.empty());device.vk=VK_NULL_HANDLE;
 std::puts("PASS: production descriptor payloads, RT64 RSP layouts, allocated array counts, 64-bit offsets, empty vertex/modify work and atomic update/destruction");
}
