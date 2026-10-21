#include "android_vulkan_validation_common.h"
static PFN_vkAllocateDescriptorSets originalAllocate;
static unsigned calls=0;
static bool deviceLost=false;
static VkResult injectedFailure(VkDevice device,const VkDescriptorSetAllocateInfo *info,VkDescriptorSet *sets) {
    ++calls;
    if(deviceLost && calls==1) {*sets=VK_NULL_HANDLE;return VK_ERROR_DEVICE_LOST;}
    if(calls==3) { REQUIRE(info->pNext && info->descriptorSetCount==1);*sets=VK_NULL_HANDLE;return VK_ERROR_OUT_OF_POOL_MEMORY; }
    return originalAllocate(device,info,sets);
}
int main(int argc,char **argv) {
    HostVulkanValidation validation;
    originalAllocate=vkAllocateDescriptorSets;
    if(argc==2) {deviceLost=std::strcmp(argv[1],"device-lost")==0;vkAllocateDescriptorSets=injectedFailure;}
    VulkanDevice device(&validation.interface,"llvmpipe");
    REQUIRE(false); // The opt-in production entry must exit before RT64 work.
}
