#include "../../../../../src/android_debug_log.h"
//
// plume
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION 

#include "plume_vulkan.h"
#if defined(__ANDROID__)
#include "../../../../../src/android_presentation_lifecycle.h"
#include "plume_vulkan_clear_shaders.h"
#include <unistd.h>
#include <cstddef>
#include "../../../../../src/android_descriptor_context.h"
#if defined(__LP64__)
static_assert(sizeof(VkDescriptorSetAllocateInfo) == 40 && offsetof(VkDescriptorSetAllocateInfo, pNext) == 8 &&
    offsetof(VkDescriptorSetAllocateInfo, pSetLayouts) == 32 && sizeof(VkDescriptorSetVariableDescriptorCountAllocateInfo) == 32 &&
    offsetof(VkDescriptorSetVariableDescriptorCountAllocateInfo, pNext) == 8 &&
    offsetof(VkDescriptorSetVariableDescriptorCountAllocateInfo, pDescriptorCounts) == 24,
    "Android Vulkan descriptor ABI must use the standard LP64 layout");
#endif
#if defined(TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE) && TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE
#include "plume_vulkan_descriptor_probe.h"
#if (defined(TOOIE_VULKAN_QUEUE_ISOLATION_TEST) && TOOIE_VULKAN_QUEUE_ISOLATION_TEST) || (defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK) && TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK)
#error "Descriptor allocation probe requires queue isolation and attachment fallback disabled"
#endif
#endif
#endif

#include <algorithm>
#include <cmath>
#include <climits>
#include <unordered_map>

#if defined(__ANDROID__) && defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK) && TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK
#if !defined(NDEBUG) || (defined(TOOIE_VULKAN_QUEUE_ISOLATION_TEST) && TOOIE_VULKAN_QUEUE_ISOLATION_TEST)
#error "Attachment-clear fallback comparison is forbidden in debug or queue-isolation builds"
#endif
#endif

#if DLSS_ENABLED
#   include "render/plume_dlss.h"
#endif

#ifndef NDEBUG
#   define VULKAN_VALIDATION_LAYER_ENABLED
#   define VULKAN_OBJECT_NAMES_ENABLED
#endif

// TODO:
// - Fix resource pools.

namespace plume {
    // Backend constants.

    // Required buffer alignment for acceleration structures.
    static const uint64_t AccelerationStructureBufferAlignment = 256;

    // Required buffer alignment for shader binding table.
    static const uint64_t ShaderBindingTableAlignment = 256;

    // Controls the maximum amount of native queues the backend will create per queue family.
    // Command queues are created as virtual queues on top of the native queues provided by Vulkan,
    // so they're not under the limit set by the the device or the backend.
    static const uint32_t MaxQueuesPerFamilyCount = 4;

    // Required extensions.

    static const std::unordered_set<std::string> RequiredInstanceExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#   ifdef VULKAN_OBJECT_NAMES_ENABLED
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#   endif
#   if defined(_WIN64)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#   elif defined(__ANDROID__)
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#   elif defined(__linux__)
#   if !defined(PLUME_SDL_VULKAN_ENABLED)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#   endif
#   elif defined(__APPLE__)
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
#   endif
    };

    static const std::unordered_set<std::string> OptionalInstanceExtensions = {
#   if defined(__APPLE__)
        // Tells the system Vulkan loader to enumerate portability drivers, if supported.
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#   endif
    };
    
    static const std::unordered_set<std::string> RequiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    
    static const std::unordered_set<std::string> OptionalDeviceExtensions = {
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME,
        VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME,
        VK_KHR_PRESENT_ID_EXTENSION_NAME,
        VK_KHR_PRESENT_WAIT_EXTENSION_NAME,
        VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
        // Vulkan spec requires this to be enabled if supported by the driver.
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
    };

    // Common functions.

    static uint32_t roundUp(uint32_t value, uint32_t powerOf2Alignment) {
        return (value + powerOf2Alignment - 1) & ~(powerOf2Alignment - 1);
    }

    static uint64_t roundUp(uint64_t value, uint64_t powerOf2Alignment) {
        return (value + powerOf2Alignment - 1) & ~(powerOf2Alignment - 1);
    }

    VkFormat toVk(RenderFormat format) {
        switch (format) {
        case RenderFormat::UNKNOWN:
            return VK_FORMAT_UNDEFINED;
        case RenderFormat::R32G32B32A32_TYPELESS:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case RenderFormat::R32G32B32A32_FLOAT:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case RenderFormat::R32G32B32A32_UINT:
            return VK_FORMAT_R32G32B32A32_UINT;
        case RenderFormat::R32G32B32A32_SINT:
            return VK_FORMAT_R32G32B32A32_SINT;
        case RenderFormat::R32G32B32_TYPELESS:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case RenderFormat::R32G32B32_FLOAT:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case RenderFormat::R32G32B32_UINT:
            return VK_FORMAT_R32G32B32_UINT;
        case RenderFormat::R32G32B32_SINT:
            return VK_FORMAT_R32G32B32_SINT;
        case RenderFormat::R16G16B16A16_TYPELESS:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RenderFormat::R16G16B16A16_FLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RenderFormat::R16G16B16A16_UNORM:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case RenderFormat::R16G16B16A16_UINT:
            return VK_FORMAT_R16G16B16A16_UINT;
        case RenderFormat::R16G16B16A16_SNORM:
            return VK_FORMAT_R16G16B16A16_SNORM;
        case RenderFormat::R16G16B16A16_SINT:
            return VK_FORMAT_R16G16B16A16_SINT;
        case RenderFormat::R32G32_TYPELESS:
            return VK_FORMAT_R32G32_SFLOAT;
        case RenderFormat::R32G32_FLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        case RenderFormat::R8G8B8A8_TYPELESS:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case RenderFormat::R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case RenderFormat::R8G8B8A8_UINT:
            return VK_FORMAT_R8G8B8A8_UINT;
        case RenderFormat::R8G8B8A8_SNORM:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case RenderFormat::R8G8B8A8_SINT:
            return VK_FORMAT_R8G8B8A8_SINT;
        case RenderFormat::B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case RenderFormat::R16G16_TYPELESS:
            return VK_FORMAT_R16G16_SFLOAT;
        case RenderFormat::R16G16_FLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case RenderFormat::R16G16_UNORM:
            return VK_FORMAT_R16G16_UNORM;
        case RenderFormat::R16G16_UINT:
            return VK_FORMAT_R16G16_UINT;
        case RenderFormat::R16G16_SNORM:
            return VK_FORMAT_R16G16_SNORM;
        case RenderFormat::R16G16_SINT:
            return VK_FORMAT_R16G16_SINT;
        case RenderFormat::R32_TYPELESS:
            return VK_FORMAT_R32_SFLOAT;
        case RenderFormat::D32_FLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case RenderFormat::D32_FLOAT_S8_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case RenderFormat::R32_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case RenderFormat::R32_UINT:
            return VK_FORMAT_R32_UINT;
        case RenderFormat::R32_SINT:
            return VK_FORMAT_R32_SINT;
        case RenderFormat::R8G8_TYPELESS:
            return VK_FORMAT_R8G8_UNORM;
        case RenderFormat::R8G8_UNORM:
            return VK_FORMAT_R8G8_UNORM;
        case RenderFormat::R8G8_UINT:
            return VK_FORMAT_R8G8_UINT;
        case RenderFormat::R8G8_SNORM:
            return VK_FORMAT_R8G8_SNORM;
        case RenderFormat::R8G8_SINT:
            return VK_FORMAT_R8G8_SINT;
        case RenderFormat::R16_TYPELESS:
            return VK_FORMAT_R16_SFLOAT;
        case RenderFormat::R16_FLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case RenderFormat::D16_UNORM:
            return VK_FORMAT_D16_UNORM;
        case RenderFormat::R16_UNORM:
            return VK_FORMAT_R16_UNORM;
        case RenderFormat::R16_UINT:
            return VK_FORMAT_R16_UINT;
        case RenderFormat::R16_SNORM:
            return VK_FORMAT_R16_SNORM;
        case RenderFormat::R16_SINT:
            return VK_FORMAT_R16_SINT;
        case RenderFormat::R8_TYPELESS:
            return VK_FORMAT_R8_UNORM;
        case RenderFormat::R8_UNORM:
            return VK_FORMAT_R8_UNORM;
        case RenderFormat::R8_UINT:
            return VK_FORMAT_R8_UINT;
        case RenderFormat::R8_SNORM:
            return VK_FORMAT_R8_SNORM;
        case RenderFormat::R8_SINT:
            return VK_FORMAT_R8_SINT;
        case RenderFormat::BC1_TYPELESS:
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case RenderFormat::BC1_UNORM:
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case RenderFormat::BC1_UNORM_SRGB:
            return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case RenderFormat::BC2_TYPELESS:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case RenderFormat::BC2_UNORM:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case RenderFormat::BC2_UNORM_SRGB:
            return VK_FORMAT_BC2_SRGB_BLOCK;
        case RenderFormat::BC3_TYPELESS:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case RenderFormat::BC3_UNORM:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case RenderFormat::BC3_UNORM_SRGB:
            return VK_FORMAT_BC3_SRGB_BLOCK;
        case RenderFormat::BC4_TYPELESS:
            return VK_FORMAT_BC4_UNORM_BLOCK;
        case RenderFormat::BC4_UNORM:
            return VK_FORMAT_BC4_UNORM_BLOCK;
        case RenderFormat::BC4_SNORM:
            return VK_FORMAT_BC4_SNORM_BLOCK;
        case RenderFormat::BC5_TYPELESS:
            return VK_FORMAT_BC5_UNORM_BLOCK;
        case RenderFormat::BC5_UNORM:
            return VK_FORMAT_BC5_UNORM_BLOCK;
        case RenderFormat::BC5_SNORM:
            return VK_FORMAT_BC5_SNORM_BLOCK;
        case RenderFormat::BC6H_TYPELESS:
            return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case RenderFormat::BC6H_UF16:
            return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case RenderFormat::BC6H_SF16:
            return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case RenderFormat::BC7_TYPELESS:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        case RenderFormat::BC7_UNORM:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        case RenderFormat::BC7_UNORM_SRGB:
            return VK_FORMAT_BC7_SRGB_BLOCK;
        default:
            assert(false && "Unknown format.");
            return VK_FORMAT_UNDEFINED;
        }
    }

    static VkImageType toImageType(RenderTextureDimension dimension) {
        switch (dimension) {
        case RenderTextureDimension::TEXTURE_1D:
            return VK_IMAGE_TYPE_1D;
        case RenderTextureDimension::TEXTURE_2D:
            return VK_IMAGE_TYPE_2D;
        case RenderTextureDimension::TEXTURE_3D:
            return VK_IMAGE_TYPE_3D;
        default:
            assert(false && "Unknown resource dimension.");
            return VK_IMAGE_TYPE_MAX_ENUM;
        }
    }

    static VkImageViewType toImageViewType(RenderTextureViewDimension dimension, uint16_t arraySize) {
        switch (dimension) {
        case RenderTextureViewDimension::TEXTURE_1D:
            return arraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        case RenderTextureViewDimension::TEXTURE_2D:
            return arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        case RenderTextureViewDimension::TEXTURE_3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case RenderTextureViewDimension::TEXTURE_CUBE:
            return arraySize > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        default:
            assert(false && "Unknown resource dimension.");
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }
    }

    static VkImageTiling toVk(RenderTextureArrangement arrangement) {
        switch (arrangement) {
        case RenderTextureArrangement::UNKNOWN:
            return VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
        case RenderTextureArrangement::ROW_MAJOR:
            return VkImageTiling::VK_IMAGE_TILING_LINEAR;
        default:
            assert(false && "Unknown texture arrangement.");
            return VkImageTiling::VK_IMAGE_TILING_MAX_ENUM;
        }
    }

    static VkVertexInputRate toVk(RenderInputSlotClassification classification) {
        switch (classification) {
        case RenderInputSlotClassification::PER_VERTEX_DATA:
            return VK_VERTEX_INPUT_RATE_VERTEX;
        case RenderInputSlotClassification::PER_INSTANCE_DATA:
            return VK_VERTEX_INPUT_RATE_INSTANCE;
        default:
            assert(false && "Unknown input slot classification.");
            return VK_VERTEX_INPUT_RATE_MAX_ENUM;
        }
    }

    static VkCullModeFlags toVk(RenderCullMode cullMode) {
        switch (cullMode) {
        case RenderCullMode::NONE:
            return VK_CULL_MODE_NONE;
        case RenderCullMode::FRONT:
            return VK_CULL_MODE_FRONT_BIT;
        case RenderCullMode::BACK:
            return VK_CULL_MODE_BACK_BIT;
        default:
            assert(false && "Unknown cull mode.");
            return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
        }
    }

    static VkFrontFace toVk(RenderFrontFace frontFace) {
        switch (frontFace) {
        case RenderFrontFace::CLOCKWISE:
            return VK_FRONT_FACE_CLOCKWISE;
        case RenderFrontFace::COUNTER_CLOCKWISE:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default:
            assert(false && "Unknown front face.");
            return VK_FRONT_FACE_CLOCKWISE;
        }
    }

    static VkPrimitiveTopology toVk(RenderPrimitiveTopology topology) {
        switch (topology) {
        case RenderPrimitiveTopology::POINT_LIST:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case RenderPrimitiveTopology::LINE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case RenderPrimitiveTopology::LINE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case RenderPrimitiveTopology::TRIANGLE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; 
        case RenderPrimitiveTopology::TRIANGLE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;  
        case RenderPrimitiveTopology::TRIANGLE_FAN:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        default:
            assert(false && "Unknown primitive topology type.");
            return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
        }
    }

    static VkBlendFactor toVk(RenderBlend blend) {
        switch (blend) {
        case RenderBlend::ZERO:
            return VK_BLEND_FACTOR_ZERO;
        case RenderBlend::ONE:
            return VK_BLEND_FACTOR_ONE;
        case RenderBlend::SRC_COLOR:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case RenderBlend::INV_SRC_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case RenderBlend::SRC_ALPHA:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case RenderBlend::INV_SRC_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case RenderBlend::DEST_ALPHA:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case RenderBlend::INV_DEST_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case RenderBlend::DEST_COLOR:
            return VK_BLEND_FACTOR_DST_COLOR;
        case RenderBlend::INV_DEST_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case RenderBlend::SRC_ALPHA_SAT:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case RenderBlend::BLEND_FACTOR:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case RenderBlend::INV_BLEND_FACTOR:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case RenderBlend::SRC1_COLOR:
            return VK_BLEND_FACTOR_SRC1_COLOR;
        case RenderBlend::INV_SRC1_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case RenderBlend::SRC1_ALPHA:
            return VK_BLEND_FACTOR_SRC1_ALPHA;
        case RenderBlend::INV_SRC1_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
        default:
            assert(false && "Unknown blend factor.");
            return VK_BLEND_FACTOR_MAX_ENUM;
        }
    }

    static VkBlendOp toVk(RenderBlendOperation operation) {
        switch (operation) {
        case RenderBlendOperation::ADD:
            return VK_BLEND_OP_ADD;
        case RenderBlendOperation::SUBTRACT:
            return VK_BLEND_OP_SUBTRACT;
        case RenderBlendOperation::REV_SUBTRACT:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case RenderBlendOperation::MIN:
            return VK_BLEND_OP_MIN;
        case RenderBlendOperation::MAX:
            return VK_BLEND_OP_MAX;
        default:
            assert(false && "Unknown blend operation.");
            return VK_BLEND_OP_MAX_ENUM;
        }
    }

    static VkLogicOp toVk(RenderLogicOperation operation) {
        switch (operation) {
        case RenderLogicOperation::CLEAR:
            return VK_LOGIC_OP_CLEAR;
        case RenderLogicOperation::SET:
            return VK_LOGIC_OP_SET;
        case RenderLogicOperation::COPY:
            return VK_LOGIC_OP_COPY;
        case RenderLogicOperation::COPY_INVERTED:
            return VK_LOGIC_OP_COPY_INVERTED;
        case RenderLogicOperation::NOOP:
            return VK_LOGIC_OP_NO_OP;
        case RenderLogicOperation::INVERT:
            return VK_LOGIC_OP_INVERT;
        case RenderLogicOperation::AND:
            return VK_LOGIC_OP_AND;
        case RenderLogicOperation::NAND:
            return VK_LOGIC_OP_NAND;
        case RenderLogicOperation::OR:
            return VK_LOGIC_OP_OR;
        case RenderLogicOperation::NOR:
            return VK_LOGIC_OP_NOR;
        case RenderLogicOperation::XOR:
            return VK_LOGIC_OP_XOR;
        case RenderLogicOperation::EQUIV:
            return VK_LOGIC_OP_EQUIVALENT;
        case RenderLogicOperation::AND_REVERSE:
            return VK_LOGIC_OP_AND_REVERSE;
        case RenderLogicOperation::AND_INVERTED:
            return VK_LOGIC_OP_AND_INVERTED;
        case RenderLogicOperation::OR_REVERSE:
            return VK_LOGIC_OP_OR_REVERSE;
        case RenderLogicOperation::OR_INVERTED:
            return VK_LOGIC_OP_OR_INVERTED;
        default:
            assert(false && "Unknown logic operation.");
            return VK_LOGIC_OP_MAX_ENUM;
        }
    }

    static VkCompareOp toVk(RenderComparisonFunction function) {
        switch (function) {
        case RenderComparisonFunction::NEVER:
            return VK_COMPARE_OP_NEVER;
        case RenderComparisonFunction::LESS:
            return VK_COMPARE_OP_LESS;
        case RenderComparisonFunction::EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case RenderComparisonFunction::LESS_EQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case RenderComparisonFunction::GREATER:
            return VK_COMPARE_OP_GREATER;
        case RenderComparisonFunction::NOT_EQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        case RenderComparisonFunction::GREATER_EQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case RenderComparisonFunction::ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        default:
            assert(false && "Unknown comparison function.");
            return VK_COMPARE_OP_MAX_ENUM;
        }
    }

    static VkStencilOp toVk(RenderStencilOp operation) {
        switch (operation) {
        case RenderStencilOp::KEEP:
            return VK_STENCIL_OP_KEEP;
        case RenderStencilOp::ZERO:
            return VK_STENCIL_OP_ZERO;
        case RenderStencilOp::REPLACE:
            return VK_STENCIL_OP_REPLACE;
        case RenderStencilOp::INCREMENT_AND_CLAMP:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case RenderStencilOp::DECREMENT_AND_CLAMP:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case RenderStencilOp::INVERT:
            return VK_STENCIL_OP_INVERT;
        case RenderStencilOp::INCREMENT_AND_WRAP:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case RenderStencilOp::DECREMENT_AND_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default:
            assert(false && "Unknown stencil operation.");
            return VK_STENCIL_OP_MAX_ENUM;
        }
    }

    static VkDescriptorType toVk(RenderDescriptorRangeType type) {
        switch (type) {
        case RenderDescriptorRangeType::CONSTANT_BUFFER:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case RenderDescriptorRangeType::FORMATTED_BUFFER:
            return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case RenderDescriptorRangeType::TEXTURE:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case RenderDescriptorRangeType::READ_WRITE_TEXTURE:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case RenderDescriptorRangeType::SAMPLER:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case RenderDescriptorRangeType::STRUCTURED_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        default:
            assert(false && "Unknown descriptor range type.");
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    static VkFilter toVk(RenderFilter filter) {
        switch (filter) {
        case RenderFilter::NEAREST:
            return VK_FILTER_NEAREST;
        case RenderFilter::LINEAR:
            return VK_FILTER_LINEAR;
        default:
            assert(false && "Unknown filter.");
            return VK_FILTER_MAX_ENUM;
        }
    }

    static VkSamplerMipmapMode toVk(RenderMipmapMode mode) {
        switch (mode) {
        case RenderMipmapMode::NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case RenderMipmapMode::LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:
            assert(false && "Unknown mipmap mode.");
            return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
        }
    }

    static VkSamplerAddressMode toVk(RenderTextureAddressMode mode, bool samplerMirrorClampToEdgeSupported) {
        switch (mode) {
        case RenderTextureAddressMode::WRAP:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case RenderTextureAddressMode::MIRROR:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case RenderTextureAddressMode::CLAMP:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case RenderTextureAddressMode::BORDER:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case RenderTextureAddressMode::MIRROR_ONCE:
            assert(samplerMirrorClampToEdgeSupported && "MIRROR_ONCE is not supported when samplerMirrorClampToEdge is not supported.");
            return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default:
            assert(false && "Unknown texture address mode.");
            return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
        }
    }

    static VkBorderColor toVk(RenderBorderColor color) {
        switch (color) {
        case RenderBorderColor::TRANSPARENT_BLACK:
            return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case RenderBorderColor::OPAQUE_BLACK:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case RenderBorderColor::OPAQUE_WHITE:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        default:
            assert(false && "Unknown border color.");
            return VK_BORDER_COLOR_MAX_ENUM;
        }
    }

    static VkAccelerationStructureTypeKHR toVk(RenderAccelerationStructureType type) {
        switch (type) {
        case RenderAccelerationStructureType::TOP_LEVEL:
            return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        case RenderAccelerationStructureType::BOTTOM_LEVEL:
            return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        default:
            assert(false && "Unknown acceleration structure type.");
            return VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
        }
    }
    
    static VkPipelineStageFlags toStageFlags(RenderBarrierStages stages, bool geometrySupported, bool rtSupported) {
        VkPipelineStageFlags flags = 0;

        if (stages & RenderBarrierStage::GRAPHICS) {
            flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            flags |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            if (geometrySupported) {
                flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
            }
        }

        if (stages & RenderBarrierStage::COMPUTE) {
            flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            if (rtSupported) {
                flags |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
                flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            }
        }

        if (stages & RenderBarrierStage::COPY) {
            flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            flags |= VK_PIPELINE_STAGE_HOST_BIT;
        }

        return flags;
    }

    static VkShaderStageFlagBits toStage(RenderRaytracingPipelineLibrarySymbolType type) {
        switch (type) {
        case RenderRaytracingPipelineLibrarySymbolType::RAYGEN:
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case RenderRaytracingPipelineLibrarySymbolType::MISS:
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        case RenderRaytracingPipelineLibrarySymbolType::CLOSEST_HIT:
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case RenderRaytracingPipelineLibrarySymbolType::ANY_HIT:
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case RenderRaytracingPipelineLibrarySymbolType::INTERSECTION:
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case RenderRaytracingPipelineLibrarySymbolType::CALLABLE:
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        default:
            assert(false && "Unknown raytracing pipeline library symbol type.");
            return VkShaderStageFlagBits(0);
        }
    }

    static uint32_t toFamilyIndex(RenderCommandListType type) {
        switch (type) {
        case RenderCommandListType::DIRECT:
            return 0;
        case RenderCommandListType::COMPUTE:
            return 1;
        case RenderCommandListType::COPY:
            return 2;
        default:
            assert(false && "Unknown command list type.");
            return 0;
        }
    }

    static VkIndexType toIndexType(RenderFormat format) {
        switch (format) {
        case RenderFormat::R8_UINT:
            return VK_INDEX_TYPE_UINT8_EXT;
        case RenderFormat::R16_UINT:
            return VK_INDEX_TYPE_UINT16;
        case RenderFormat::R32_UINT:
            return VK_INDEX_TYPE_UINT32;
        default:
            assert(false && "Format is not supported as an index type.");
            return VK_INDEX_TYPE_MAX_ENUM;
        }
    }

    static VkBuildAccelerationStructureFlagsKHR toRTASBuildFlags(bool preferFastBuild, bool preferFastTrace) {
        VkBuildAccelerationStructureFlagsKHR flags = 0;
        flags |= preferFastBuild ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR : 0;
        flags |= preferFastTrace ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR : 0;
        return flags;
    }
    
    static VkImageLayout toImageLayout(RenderTextureLayout layout) {
        switch (layout) {
        case RenderTextureLayout::UNKNOWN:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        case RenderTextureLayout::GENERAL:
            return VK_IMAGE_LAYOUT_GENERAL;
        case RenderTextureLayout::SHADER_READ:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case RenderTextureLayout::COLOR_WRITE:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case RenderTextureLayout::DEPTH_WRITE:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case RenderTextureLayout::DEPTH_READ:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case RenderTextureLayout::COPY_SOURCE:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case RenderTextureLayout::COPY_DEST:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case RenderTextureLayout::RESOLVE_SOURCE:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case RenderTextureLayout::RESOLVE_DEST:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case RenderTextureLayout::PRESENT:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default:
            assert(false && "Unknown texture layout.");
            return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    static VkImageAspectFlags toViewAspectFlags(const RenderTextureFlags flags) {
        return (flags & RenderTextureFlag::DEPTH_TARGET) != 0 ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    }

    static VkImageAspectFlags toAspectFlags(const RenderFormat format, const RenderTextureFlags flags) {
        VkImageAspectFlags aspect = toViewAspectFlags(flags);
        if ((flags & RenderTextureFlag::DEPTH_TARGET) != 0 && RenderFormatIsStencil(format)) {
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        return aspect;
    }

    static VkComponentSwizzle toVk(RenderSwizzle swizzle) {
        switch (swizzle) {
        case RenderSwizzle::IDENTITY:
            return VK_COMPONENT_SWIZZLE_IDENTITY;
        case RenderSwizzle::ZERO:
            return VK_COMPONENT_SWIZZLE_ZERO;
        case RenderSwizzle::ONE:
            return VK_COMPONENT_SWIZZLE_ONE;
        case RenderSwizzle::R:
            return VK_COMPONENT_SWIZZLE_R;
        case RenderSwizzle::G:
            return VK_COMPONENT_SWIZZLE_G;
        case RenderSwizzle::B:
            return VK_COMPONENT_SWIZZLE_B;
        case RenderSwizzle::A:
            return VK_COMPONENT_SWIZZLE_A;
        default:
            assert(false && "Unknown swizzle type.");
            return VK_COMPONENT_SWIZZLE_IDENTITY;
        }
    }

    static RenderDeviceType toDeviceType(VkPhysicalDeviceType type) {
        switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return RenderDeviceType::INTEGRATED;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return RenderDeviceType::DISCRETE;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return RenderDeviceType::VIRTUAL;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return RenderDeviceType::CPU;
        default:
            return RenderDeviceType::UNKNOWN;
        }
    }

    static void setObjectName(VkDevice device, VkObjectType objectType, uint64_t object, const std::string &name) {
#   ifdef VULKAN_OBJECT_NAMES_ENABLED
        VkDebugUtilsObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = objectType;
        nameInfo.objectHandle = object;
        nameInfo.pObjectName = name.c_str();
        VkResult res = vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkSetDebugUtilsObjectNameEXT failed with error code 0x%X.\n", res);
            return;
        }
#   endif
    }

    static void fillSpecInfo(const RenderSpecConstant *specConstants, uint32_t specConstantsCount,
        VkSpecializationInfo &specInfo, VkSpecializationMapEntry *specEntries, uint32_t *specData)
    {
        for (uint32_t i = 0; i < specConstantsCount; i++) {
            VkSpecializationMapEntry &entry = specEntries[i];
            entry.constantID = specConstants[i].index;
            entry.offset = i * sizeof(uint32_t);
            entry.size = sizeof(uint32_t);
            specData[i] = specConstants[i].value;
        }

        specInfo.mapEntryCount = specConstantsCount;
        specInfo.pMapEntries = specEntries;
        specInfo.dataSize = specConstantsCount * sizeof(uint32_t);
        specInfo.pData = specData;
    }

    // Underlying implementation for popcount
    // https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
    static int numberOfSetBits(uint32_t i) {
        i = i - ((i >> 1) & 0x55555555);
        i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
        return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
    }

    // VulkanBuffer

    VulkanBuffer::VulkanBuffer(VulkanDevice *device, VulkanPool *pool, const RenderBufferDesc &desc) {
        assert(device != nullptr);

        this->device = device;
        this->pool = pool;
        this->desc = desc;

#if defined(__ANDROID__)
        // STORAGE permits byte-address/structured access as well. Only an
        // explicitly writable formatted buffer needs STORAGE_TEXEL_BUFFER.
        const RenderBufferFlags storageFormattedMask = RenderBufferFlag::STORAGE | RenderBufferFlag::FORMATTED | RenderBufferFlag::UNORDERED_ACCESS;
#else
        const RenderBufferFlags storageFormattedMask = RenderBufferFlag::STORAGE | RenderBufferFlag::FORMATTED;
#endif
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.size;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.usage |= (desc.flags & RenderBufferFlag::VERTEX) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0;
        bufferInfo.usage |= (desc.flags & RenderBufferFlag::INDEX) ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : 0;
        bufferInfo.usage |= (desc.flags & RenderBufferFlag::STORAGE) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0;
        bufferInfo.usage |= (desc.flags & RenderBufferFlag::CONSTANT) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0;
        bufferInfo.usage |= (desc.flags & RenderBufferFlag::FORMATTED) ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : 0;
        bufferInfo.usage |= ((desc.flags & storageFormattedMask) == storageFormattedMask) ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : 0;
        bufferInfo.usage |= (desc.flags & RenderBufferFlag::ACCELERATION_STRUCTURE) ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR : 0;
        bufferInfo.usage |= (desc.flags & RenderBufferFlag::ACCELERATION_STRUCTURE_SCRATCH) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0;
        bufferInfo.usage |= (desc.flags & RenderBufferFlag::ACCELERATION_STRUCTURE_INPUT) ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0;
        bufferInfo.usage |= (desc.flags & RenderBufferFlag::SHADER_BINDING_TABLE) ? VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR : 0;
        
        const uint32_t deviceAddressMask = RenderBufferFlag::CONSTANT | RenderBufferFlag::ACCELERATION_STRUCTURE | RenderBufferFlag::ACCELERATION_STRUCTURE_SCRATCH | RenderBufferFlag::ACCELERATION_STRUCTURE_INPUT | RenderBufferFlag::SHADER_BINDING_TABLE;
        const bool useDeviceAddress = device->capabilities.bufferDeviceAddress && (desc.flags & deviceAddressMask);
        bufferInfo.usage |= useDeviceAddress ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0;
        
        VmaAllocationCreateInfo createInfo = {};
        /* TODO: Debug pools.
        createInfo.pool = (pool != nullptr) ? pool->vk : VK_NULL_HANDLE;
        */
        createInfo.usage = VMA_MEMORY_USAGE_AUTO;

        switch (desc.heapType) {
        case RenderHeapType::DEFAULT:
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            createInfo.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case RenderHeapType::UPLOAD:
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            createInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;
        case RenderHeapType::READBACK:
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            createInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            break;
        case RenderHeapType::GPU_UPLOAD:
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            createInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            createInfo.requiredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            break;
        default:
            assert(false && "Unknown heap type.");
            break;
        }

        if (desc.committed) {
            createInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkDeviceSize minAlignment = 0;

        // The specification imposes an alignment requirement for SBTs.
        if (desc.flags & RenderBufferFlag::SHADER_BINDING_TABLE) {
            minAlignment = device->rtPipelineProperties.shaderGroupBaseAlignment;
        }

#if defined(__ANDROID__)
        bufferUsage = bufferInfo.usage;
#endif
        VkResult res;
        if (minAlignment > 0) {
            res = vmaCreateBufferWithAlignment(device->allocator, &bufferInfo, &createInfo, minAlignment, &vk, &allocation, &allocationInfo);
        }
        else {
            res = vmaCreateBuffer(device->allocator, &bufferInfo, &createInfo, &vk, &allocation, &allocationInfo);
        }

#if defined(__ANDROID__)
        if (res != VK_SUCCESS) vk = VK_NULL_HANDLE;
        device->descriptorResources.track(this, VK_OBJECT_TYPE_BUFFER, uint64_t(vk));
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vmaCreateBuffer failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanBuffer::~VulkanBuffer() {
#if defined(__ANDROID__)
        std::unique_lock<std::recursive_mutex> lock;
        if (device) { lock = std::unique_lock(device->descriptorResources.mutex); device->descriptorResources.erase(this); }
#endif
        if (vk != VK_NULL_HANDLE) {
            vmaDestroyBuffer(device->allocator, vk, allocation);
        }
    }

    void *VulkanBuffer::map(uint32_t subresource, const RenderRange *readRange) {
        void *data = nullptr;
        VkResult res = vmaMapMemory(device->allocator, allocation, &data);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vmaMapMemory failed with error code 0x%X.\n", res);
            return nullptr;
        }

        return data;
    }

    void VulkanBuffer::unmap(uint32_t subresource, const RenderRange *writtenRange) {
        vmaUnmapMemory(device->allocator, allocation);
    }

    std::unique_ptr<RenderBufferFormattedView> VulkanBuffer::createBufferFormattedView(RenderFormat format) {
        return std::make_unique<VulkanBufferFormattedView>(this, format);
    }

    void VulkanBuffer::setName(const std::string &name) {
        setObjectName(device->vk, VK_OBJECT_TYPE_BUFFER, uint64_t(vk), name);
    }

    uint64_t VulkanBuffer::getDeviceAddress() const {
        assert((desc.flags & RenderBufferFlag::DEVICE_ADDRESSABLE) != 0 && "Buffer must have been created with GPU_ADDRESSABLE flag.");
        VkBufferDeviceAddressInfo info;
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.pNext = nullptr;
        info.buffer = vk;
        return vkGetBufferDeviceAddress(device->vk, &info);
    }

    // VulkanBufferFormattedView

    VulkanBufferFormattedView::VulkanBufferFormattedView(VulkanBuffer *buffer, RenderFormat format) {
        assert(buffer != nullptr);
        assert((buffer->desc.flags & RenderBufferFlag::FORMATTED) && "Buffer must allow formatted views.");

        this->buffer = buffer;
#if defined(__ANDROID__)
        this->format = format;
        viewDevice = buffer->device;
        const std::lock_guard lock(viewDevice->descriptorResources.mutex);
        viewDevice->descriptorResources.track(this, VK_OBJECT_TYPE_BUFFER_VIEW, 0, buffer);
        VkFormatProperties properties = {};
        vkGetPhysicalDeviceFormatProperties(viewDevice->physicalDevice, toVk(format), &properties);
        const VkFormatFeatureFlags required =
            ((buffer->bufferUsage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) ? VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT : 0) |
            ((buffer->bufferUsage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) ? VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT : 0);
        const uint32_t elementSize = RenderFormatSize(format);
        const uint64_t maxElements = viewDevice->physicalDeviceProperties.limits.maxTexelBufferElements;
        const char *reason = !buffer->vk ? "null-buffer-handle" :
            !required ? "missing-texel-buffer-usage" :
            (properties.bufferFeatures & required) != required ? "format-texel-buffer-unsupported" :
            (!elementSize || !buffer->desc.size || buffer->desc.size % elementSize) ? "invalid-texel-buffer-range" :
            (maxElements && buffer->desc.size / elementSize > maxElements) ? "texel-buffer-element-limit" : nullptr;
        if (reason) {
            char record[384];
            std::snprintf(record, sizeof(record), "Descriptor buffer view rejected: reason=%s ctx=%s format=%u features=0x%X required=0x%X usage=0x%X bytes=%llu elementBytes=%u maxElements=%llu",
                reason, tooie::android::descriptorContext(), unsigned(toVk(format)), properties.bufferFeatures, required, buffer->bufferUsage,
                (unsigned long long)buffer->desc.size, elementSize, (unsigned long long)maxElements);
            tooie_android_debug_log(record);
            std::fprintf(stderr, "%s\n", record);
            return; // Alive failed view; a required descriptor update will reject it.
        }
#endif

        VkBufferViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        createInfo.buffer = buffer->vk;
        createInfo.format = toVk(format);
        createInfo.offset = 0;
        createInfo.range = buffer->desc.size;

        VkResult res = vkCreateBufferView(buffer->device->vk, &createInfo, nullptr, &vk);
#if defined(__ANDROID__)
        if (res != VK_SUCCESS) vk = VK_NULL_HANDLE;
        buffer->device->descriptorResources.track(this, VK_OBJECT_TYPE_BUFFER_VIEW, uint64_t(vk), buffer);
        if (res != VK_SUCCESS) {
            char record[256];
            std::snprintf(record, sizeof(record), "Descriptor buffer view creation failed: result=%d buffer=%llx format=%u bytes=%llu flags=0x%X",
                int(res), (unsigned long long)buffer->vk, unsigned(toVk(format)), (unsigned long long)buffer->desc.size, unsigned(buffer->desc.flags));
            tooie_android_debug_log(record);
        }
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateBufferView failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanBufferFormattedView::~VulkanBufferFormattedView() {
#if defined(__ANDROID__)
        const std::lock_guard lock(viewDevice->descriptorResources.mutex);
        viewDevice->descriptorResources.erase(this);
#endif
        if (vk != VK_NULL_HANDLE) {
#if defined(__ANDROID__)
            vkDestroyBufferView(viewDevice->vk, vk, nullptr);
#else
            vkDestroyBufferView(buffer->device->vk, vk, nullptr);
#endif
        }
    }

    // VulkanTexture

#if defined(__ANDROID__)
    bool VulkanTexture::validateImageConfiguration(VulkanDevice *device, const VkImageCreateInfo &info) {
        VkImageFormatProperties limits = {};
        const VkResult result = vkGetPhysicalDeviceImageFormatProperties(device->physicalDevice,
            info.format, info.imageType, info.tiling, info.usage, info.flags, &limits);
        const char *reason = nullptr;
        if (result != VK_SUCCESS) { reason = "image-format-query-failed"; limits = {}; }
        else if (!(limits.sampleCounts & info.samples)) reason = "sample-count-unsupported";
        else if (!info.extent.width || !info.extent.height || !info.extent.depth ||
            info.extent.width > limits.maxExtent.width || info.extent.height > limits.maxExtent.height ||
            info.extent.depth > limits.maxExtent.depth) reason = "image-extent-unsupported";
        else if (!info.mipLevels || info.mipLevels > limits.maxMipLevels) reason = "mip-level-count-unsupported";
        else if (!info.arrayLayers || info.arrayLayers > limits.maxArrayLayers) reason = "array-layer-count-unsupported";
        if (reason != nullptr) {
            char message[512];
            std::snprintf(message, sizeof(message),
                "Image configuration rejected: reason=%s result=%d format=%u type=%u tiling=%u usage=0x%X flags=0x%X "
                "samples=%u supportedSamples=0x%X extent=%ux%ux%u maxExtent=%ux%ux%u mips=%u maxMips=%u layers=%u maxLayers=%u",
                reason, int(result), unsigned(info.format), unsigned(info.imageType), unsigned(info.tiling), info.usage,
                info.flags, unsigned(info.samples), limits.sampleCounts, info.extent.width, info.extent.height,
                info.extent.depth, limits.maxExtent.width, limits.maxExtent.height, limits.maxExtent.depth,
                info.mipLevels, limits.maxMipLevels, info.arrayLayers, limits.maxArrayLayers);
            tooie_android_debug_log(message);
            std::fprintf(stderr, "%s\n", message);
            return false;
        }
        return true;
    }
#endif

    VulkanTexture::VulkanTexture(VulkanDevice *device, VulkanPool *pool, const RenderTextureDesc &desc) {
        assert(device != nullptr);

        this->device = device;
        this->pool = pool;
        this->desc = desc;
        this->ownership = true;

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = toImageType(desc.dimension);
        imageInfo.format = toVk(desc.format);
        imageInfo.extent.width = uint32_t(desc.width);
        imageInfo.extent.height = desc.height;
        imageInfo.extent.depth = desc.depth;
        imageInfo.mipLevels = desc.mipLevels;
        imageInfo.arrayLayers = desc.arraySize;
        imageInfo.samples = VkSampleCountFlagBits(desc.multisampling.sampleCount);
        imageInfo.tiling = toVk(desc.textureArrangement);
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage |= (desc.flags & RenderTextureFlag::RENDER_TARGET) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
        imageInfo.usage |= (desc.flags & RenderTextureFlag::DEPTH_TARGET) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
        imageInfo.usage |= (desc.flags & RenderTextureFlag::STORAGE) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;

        if (desc.multisampling.sampleLocationsEnabled && (desc.flags & RenderTextureFlag::DEPTH_TARGET)) {
            imageInfo.flags |= VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT;
        }

        if (desc.flags & RenderTextureFlag::CUBE) {
            imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

#if defined(__ANDROID__)
        // Validate the complete color-target configuration, including the UI's
        // MSAA target, before VMA can allocate an unsupported image.
        if ((imageInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) &&
            !validateImageConfiguration(device, imageInfo)) {
            std::_Exit(EXIT_FAILURE);
        }
#endif
        imageFormat = imageInfo.format;
        imageUsage = imageInfo.usage;
        fillSubresourceRange();

        VmaAllocationCreateInfo createInfo = {};
        createInfo.pool = (pool != nullptr) ? pool->vk : VK_NULL_HANDLE;
        createInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (desc.committed) {
            createInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkResult res = vmaCreateImage(device->allocator, &imageInfo, &createInfo, &vk, &allocation, &allocationInfo);
#if defined(__ANDROID__)
        if (res != VK_SUCCESS) vk = VK_NULL_HANDLE;
        device->descriptorResources.track(this, VK_OBJECT_TYPE_IMAGE, uint64_t(vk));
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vmaCreateImage failed with error code 0x%X.\n", res);
            return;
        }

        createImageView(imageInfo.format);
    }

    VulkanTexture::VulkanTexture(VulkanDevice *device, VkImage image) {
        assert(device != nullptr);
        assert(image != VK_NULL_HANDLE);

        this->device = device;
        vk = image;
#if defined(__ANDROID__)
        device->descriptorResources.track(this, VK_OBJECT_TYPE_IMAGE, uint64_t(vk));
#endif
    }

    VulkanTexture::~VulkanTexture() {
#if defined(__ANDROID__)
        std::unique_lock<std::recursive_mutex> lock;
        if (device) { lock = std::unique_lock(device->descriptorResources.mutex); device->descriptorResources.erase(this); }
#endif
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device->vk, imageView, nullptr);
        }

        if (ownership && (vk != VK_NULL_HANDLE)) {
            vmaDestroyImage(device->allocator, vk, allocation);
        }
    }
    
    void VulkanTexture::createImageView(VkFormat format) {
        VkImageView view = VK_NULL_HANDLE;
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vk;
        viewInfo.viewType = toImageViewType(RenderTextureDimensionToView(desc.dimension), desc.arraySize);
        viewInfo.format = format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange = imageSubresourceRange;
        
        VkResult res = vkCreateImageView(device->vk, &viewInfo, nullptr, &imageView);
#if defined(__ANDROID__)
        if (res != VK_SUCCESS) imageView = VK_NULL_HANDLE;
        device->descriptorResources.track(this, VK_OBJECT_TYPE_IMAGE, uint64_t(vk));
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateImageView failed with error code 0x%X.\n", res);
            return;
        }
    }

    std::unique_ptr<RenderTextureView> VulkanTexture::createTextureView(const RenderTextureViewDesc &desc) const {
        return std::make_unique<VulkanTextureView>(this, desc);
    }

    void VulkanTexture::setName(const std::string &name) {
        setObjectName(device->vk, VK_OBJECT_TYPE_IMAGE, uint64_t(vk), name);
    }

    void VulkanTexture::fillSubresourceRange() {
        imageSubresourceRange.aspectMask = toViewAspectFlags(desc.flags);
        imageSubresourceRange.baseMipLevel = 0;
        imageSubresourceRange.levelCount = desc.mipLevels;
        imageSubresourceRange.baseArrayLayer = 0;
        imageSubresourceRange.layerCount = desc.arraySize;
    }

    // VulkanTextureView

    VulkanTextureView::VulkanTextureView(const VulkanTexture *texture, const RenderTextureViewDesc &desc) {
        assert(texture != nullptr);
        assert(desc.mipSlice < texture->desc.mipLevels);
        assert(desc.arrayIndex < texture->desc.arraySize);

        this->texture = texture;
        this->desc = desc;
#if defined(__ANDROID__)
        viewDevice = texture->device;
#endif

        const uint32_t mipLevels = std::min(desc.mipLevels, texture->desc.mipLevels - desc.mipSlice);
        const uint32_t arraySize = std::min(desc.arraySize, texture->desc.arraySize - desc.arrayIndex);

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture->vk;
        viewInfo.viewType = toImageViewType(desc.dimension, arraySize);
        viewInfo.format = toVk(desc.format);
        viewInfo.components.r = toVk(desc.componentMapping.r);
        viewInfo.components.g = toVk(desc.componentMapping.g);
        viewInfo.components.b = toVk(desc.componentMapping.b);
        viewInfo.components.a = toVk(desc.componentMapping.a);
        viewInfo.subresourceRange.aspectMask = toViewAspectFlags(texture->desc.flags);
        viewInfo.subresourceRange.baseMipLevel = desc.mipSlice;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = desc.arrayIndex;
        viewInfo.subresourceRange.layerCount = arraySize;

        VkResult res = vkCreateImageView(texture->device->vk, &viewInfo, nullptr, &vk);
#if defined(__ANDROID__)
        if (res != VK_SUCCESS) vk = VK_NULL_HANDLE;
        texture->device->descriptorResources.track(this, VK_OBJECT_TYPE_IMAGE_VIEW, uint64_t(vk), texture);
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateImageView failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanTextureView::~VulkanTextureView() {
#if defined(__ANDROID__)
        const std::lock_guard lock(viewDevice->descriptorResources.mutex);
        viewDevice->descriptorResources.erase(this);
#endif
        if (vk != VK_NULL_HANDLE) {
#if defined(__ANDROID__)
            vkDestroyImageView(viewDevice->vk, vk, nullptr);
#else
            vkDestroyImageView(texture->device->vk, vk, nullptr);
#endif
        }
    }

    // VulkanAccelerationStructure

    VulkanAccelerationStructure::VulkanAccelerationStructure(VulkanDevice *device, const RenderAccelerationStructureDesc &desc) {
        assert(device != nullptr);
        assert(desc.buffer.ref != nullptr);

        this->device = device;
        this->type = desc.type;

        const VulkanBuffer *interfaceBuffer = static_cast<const VulkanBuffer *>(desc.buffer.ref);
        VkAccelerationStructureCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = interfaceBuffer->vk;
        createInfo.offset = desc.buffer.offset;
        createInfo.size = desc.size;
        createInfo.type = toVk(desc.type);

        VkResult res = vkCreateAccelerationStructureKHR(device->vk, &createInfo, nullptr, &vk);
#if defined(__ANDROID__)
        if (res != VK_SUCCESS) vk = VK_NULL_HANDLE;
        device->descriptorResources.track(this, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, uint64_t(vk), desc.buffer.ref);
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateAccelerationStructureKHR failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanAccelerationStructure::~VulkanAccelerationStructure() {
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        device->descriptorResources.erase(this);
#endif
        if (vk != VK_NULL_HANDLE) {
            vkDestroyAccelerationStructureKHR(device->vk, vk, nullptr);
        }
    }

    // VulkanDescriptorSetLayout

#if defined(__ANDROID__)
    [[noreturn]] static void rejectDescriptorLayout(const char *reason, uint32_t binding, uint32_t element) {
        char record[256];
        std::snprintf(record, sizeof(record), "Descriptor layout rejected: reason=%s ctx=%s pid=%d tid=%d binding=%u element=%u",
            reason, tooie::android::descriptorContext(), int(getpid()), int(gettid()), binding, element);
        tooie_android_debug_log(record);
        std::fprintf(stderr, "%s\n", record);
        std::_Exit(EXIT_FAILURE);
    }
#endif
    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice *device, const RenderDescriptorSetDesc &descriptorSetDesc) {
        assert(device != nullptr);

        this->device = device;

        // Gather immutable sampler handles.
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        if ((descriptorSetDesc.descriptorRangesCount && !descriptorSetDesc.descriptorRanges) ||
            (descriptorSetDesc.lastRangeIsBoundless && !descriptorSetDesc.descriptorRangesCount))
            rejectDescriptorLayout("invalid-layout-ranges", UINT32_MAX, 0);
        std::unordered_set<uint32_t> bindingNumbers;
        for (uint32_t i = 0; i < descriptorSetDesc.descriptorRangesCount; ++i) {
            const auto &range = descriptorSetDesc.descriptorRanges[i];
            if (!range.count || toVk(range.type) == VK_DESCRIPTOR_TYPE_MAX_ENUM)
                rejectDescriptorLayout("invalid-layout-type-or-count", range.binding, 0);
            if (!bindingNumbers.insert(range.binding).second)
                rejectDescriptorLayout("duplicate-layout-binding", range.binding, 0);
        }
        auto &samplerHandles = immutableSamplerHandles;
#else
        thread_local std::vector<VkSampler> samplerHandles;
#endif
        samplerHandles.clear();

        for (uint32_t i = 0; i < descriptorSetDesc.descriptorRangesCount; i++) {
            const RenderDescriptorRange &srcRange = descriptorSetDesc.descriptorRanges[i];
            if (srcRange.immutableSampler != nullptr) {
                for (uint32_t j = 0; j < srcRange.count; j++) {
#if defined(__ANDROID__)
                    if (!device->descriptorResources.live(srcRange.immutableSampler[j], VK_OBJECT_TYPE_SAMPLER))
                        rejectDescriptorLayout("immutable-sampler-not-live", srcRange.binding, j);
#endif
                    const VulkanSampler *interfaceSampler = static_cast<const VulkanSampler *>(srcRange.immutableSampler[j]);
#if defined(__ANDROID__)
                    if (!interfaceSampler->vk) rejectDescriptorLayout("null-immutable-sampler", srcRange.binding, j);
                    immutableSamplerResources.emplace_back(interfaceSampler, device->descriptorResources.objects.at(interfaceSampler).generation);
#endif
                    assert(interfaceSampler != nullptr);
                    samplerHandles.emplace_back(interfaceSampler->vk);
                }
            }
        }
        
        // Create bindings.
        uint32_t immutableSamplerIndex = 0;
        for (uint32_t i = 0; i < descriptorSetDesc.descriptorRangesCount; i++) {
            const RenderDescriptorRange &srcRange = descriptorSetDesc.descriptorRanges[i];
            VkDescriptorSetLayoutBinding dstBinding = {};
            dstBinding.binding = srcRange.binding;
            dstBinding.descriptorCount = srcRange.count;
            dstBinding.stageFlags = VK_SHADER_STAGE_ALL;
            dstBinding.descriptorType = toVk(srcRange.type);
            if (srcRange.immutableSampler != nullptr) {
                dstBinding.pImmutableSamplers = &samplerHandles[immutableSamplerIndex];
                immutableSamplerIndex += srcRange.count;
            }

            uint32_t indexBase = uint32_t(descriptorIndexBases.size());
            uint32_t bindingIndex = uint32_t(setBindings.size());
            for (uint32_t j = 0; j < srcRange.count; j++) {
                descriptorIndexBases.emplace_back(indexBase);
                descriptorBindingIndices.emplace_back(bindingIndex);
            }

            setBindings.emplace_back(dstBinding);
        }

        VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
        setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayoutInfo.pBindings = !setBindings.empty() ? setBindings.data() : nullptr;
        setLayoutInfo.bindingCount = uint32_t(setBindings.size());
        
#if defined(__ANDROID__)
        auto &bindingFlags = this->bindingFlags;
        bindingFlags.resize(setBindings.size(), 0);
#else
        thread_local std::vector<VkDescriptorBindingFlags> bindingFlags;
#endif
        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {};
        if (descriptorSetDesc.lastRangeIsBoundless && (descriptorSetDesc.descriptorRangesCount > 0)) {
            bindingFlags.clear();
            bindingFlags.resize(descriptorSetDesc.descriptorRangesCount, 0);
            bindingFlags[descriptorSetDesc.descriptorRangesCount - 1] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

            flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            flagsInfo.pBindingFlags = bindingFlags.data();
            flagsInfo.bindingCount = uint32_t(bindingFlags.size());

            setLayoutInfo.pNext = &flagsInfo;
            setLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        }

#if defined(__ANDROID__)
        creationFlags = setLayoutInfo.flags;
        char record[512];
        for (uint32_t i = 0; i < setBindings.size(); ++i) {
            const auto &binding = setBindings[i];
            std::snprintf(record, sizeof(record), "Descriptor layout binding: pid=%d tid=%d ctx=%s binding=%u type=%u declared=%u stages=0x%X immutable=%u flags=0x%X layoutFlags=0x%X",
                int(getpid()), int(gettid()), tooie::android::descriptorContext(), binding.binding, unsigned(binding.descriptorType), binding.descriptorCount,
                binding.stageFlags, unsigned(binding.pImmutableSamplers != nullptr), bindingFlags[i], creationFlags);
            tooie_android_debug_log(record);
        }
        if (descriptorSetDesc.lastRangeIsBoundless) {
            const auto &features = device->descriptorIndexingFeatures;
            const auto &limits = device->descriptorIndexingProperties;
            const auto &last = setBindings.back();
            std::snprintf(record, sizeof(record), "Descriptor indexing configuration: pid=%d tid=%d ctx=%s enabledVariable=%u enabledPartial=%u enabledRuntime=%u enabledSampledUAB=%u maxSetSampledUAB=%u maxStageSampledUAB=%u maxAllPoolsUAB=%u",
                int(getpid()), int(gettid()), tooie::android::descriptorContext(), features.descriptorBindingVariableDescriptorCount,
                features.descriptorBindingPartiallyBound, features.runtimeDescriptorArray, features.descriptorBindingSampledImageUpdateAfterBind,
                limits.maxDescriptorSetUpdateAfterBindSampledImages, limits.maxPerStageDescriptorUpdateAfterBindSampledImages, limits.maxUpdateAfterBindDescriptorsInAllPools);
            tooie_android_debug_log(record);
            if (!features.descriptorBindingVariableDescriptorCount || !features.descriptorBindingPartiallyBound || !features.runtimeDescriptorArray)
                rejectDescriptorLayout("descriptor-indexing-features-not-enabled", last.binding, 0);
            const bool updateSupported = last.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || last.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER
                ? features.descriptorBindingSampledImageUpdateAfterBind
                : last.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ? features.descriptorBindingStorageImageUpdateAfterBind
                : last.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? features.descriptorBindingUniformBufferUpdateAfterBind
                : last.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? features.descriptorBindingStorageBufferUpdateAfterBind
                : last.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ? features.descriptorBindingUniformTexelBufferUpdateAfterBind
                : last.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ? features.descriptorBindingStorageTexelBufferUpdateAfterBind : false;
            if (!updateSupported) rejectDescriptorLayout("update-after-bind-feature-not-enabled", last.binding, 0);
            for (size_t i = 0; i + 1 < setBindings.size(); ++i)
                if (setBindings[i].binding >= last.binding) rejectDescriptorLayout("variable-binding-not-highest", last.binding, 0);
            uint64_t sampledCount = 0;
            for (const auto &binding : setBindings)
                if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) sampledCount += binding.descriptorCount;
            if (sampledCount > limits.maxDescriptorSetUpdateAfterBindSampledImages || sampledCount > limits.maxPerStageDescriptorUpdateAfterBindSampledImages)
                rejectDescriptorLayout("sampled-image-update-after-bind-limit", last.binding, 0);
        }
        VkDescriptorSetVariableDescriptorCountLayoutSupport variableSupport = {};
        variableSupport.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT;
        VkDescriptorSetLayoutSupport support = {};
        support.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT;
        support.pNext = descriptorSetDesc.lastRangeIsBoundless ? &variableSupport : nullptr;
        vkGetDescriptorSetLayoutSupport(device->vk, &setLayoutInfo, &support);
        maxVariableDescriptorCount = variableSupport.maxVariableDescriptorCount;
        std::snprintf(record, sizeof(record), "Descriptor layout support: pid=%d tid=%d ctx=%s supported=%u bindings=%u flags=0x%X maxVariable=%u",
            int(getpid()), int(gettid()), tooie::android::descriptorContext(), support.supported, setLayoutInfo.bindingCount, creationFlags, maxVariableDescriptorCount);
        tooie_android_debug_log(record);
        if (!support.supported) rejectDescriptorLayout("descriptor-layout-unsupported", UINT32_MAX, 0);
        if (descriptorSetDesc.lastRangeIsBoundless && setBindings.back().descriptorCount > maxVariableDescriptorCount)
            rejectDescriptorLayout("variable-descriptor-layout-limit", setBindings.back().binding, 0);
#endif
        
        VkResult res = vkCreateDescriptorSetLayout(device->vk, &setLayoutInfo, nullptr, &vk);
#if defined(__ANDROID__)
        creationResult = res;
        if (res != VK_SUCCESS) vk = VK_NULL_HANDLE;
        device->descriptorResources.track(this, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, uint64_t(vk));
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateDescriptorSetLayout failed with error code 0x%X.\n", res);
#if defined(__ANDROID__)
            char record[256];
            std::snprintf(record, sizeof(record), "Descriptor layout creation failed: result=%d bindings=%u variable=%u", int(res), unsigned(setBindings.size()), unsigned(descriptorSetDesc.lastRangeIsBoundless));
            tooie_android_debug_log(record);
#endif
            return;
        }
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        device->descriptorResources.erase(this);
#endif
        if (vk != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device->vk, vk, nullptr);
        }
    }

    // VulkanPipelineLayout

    VulkanPipelineLayout::VulkanPipelineLayout(VulkanDevice *device, const RenderPipelineLayoutDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        for (uint32_t i = 0; i < desc.pushConstantRangesCount; i++) {
            const RenderPushConstantRange &srcRange = desc.pushConstantRanges[i];
            VkPushConstantRange dstRange = {};
            dstRange.size = srcRange.size;
            dstRange.offset = srcRange.offset;
            dstRange.stageFlags |= (srcRange.stageFlags & RenderShaderStageFlag::VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0;
            dstRange.stageFlags |= (srcRange.stageFlags & RenderShaderStageFlag::GEOMETRY) ? VK_SHADER_STAGE_GEOMETRY_BIT : 0;
            dstRange.stageFlags |= (srcRange.stageFlags & RenderShaderStageFlag::PIXEL) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0;
            dstRange.stageFlags |= (srcRange.stageFlags & RenderShaderStageFlag::COMPUTE) ? VK_SHADER_STAGE_COMPUTE_BIT : 0;
            dstRange.stageFlags |= (srcRange.stageFlags & RenderShaderStageFlag::RAYGEN) ? VK_SHADER_STAGE_RAYGEN_BIT_KHR : 0;
            dstRange.stageFlags |= (srcRange.stageFlags & RenderShaderStageFlag::ANY_HIT) ? VK_SHADER_STAGE_ANY_HIT_BIT_KHR : 0;
            dstRange.stageFlags |= (srcRange.stageFlags & RenderShaderStageFlag::CLOSEST_HIT) ? VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR : 0;
            dstRange.stageFlags |= (srcRange.stageFlags & RenderShaderStageFlag::MISS) ? VK_SHADER_STAGE_MISS_BIT_KHR : 0;
            dstRange.stageFlags |= (srcRange.stageFlags & RenderShaderStageFlag::CALLABLE) ? VK_SHADER_STAGE_CALLABLE_BIT_KHR : 0;
            pushConstantRanges.emplace_back(dstRange);
        }

        layoutInfo.pPushConstantRanges = !pushConstantRanges.empty() ? pushConstantRanges.data() : nullptr;
        layoutInfo.pushConstantRangeCount = uint32_t(pushConstantRanges.size());

        thread_local std::vector<VkDescriptorSetLayout> setLayoutHandles;
        setLayoutHandles.clear();

        for (uint32_t i = 0; i < desc.descriptorSetDescsCount; i++) {
            VulkanDescriptorSetLayout *setLayout = new VulkanDescriptorSetLayout(device, desc.descriptorSetDescs[i]);
            descriptorSetLayouts.emplace_back(setLayout);
            setLayoutHandles.emplace_back(setLayout->vk);
        }

        layoutInfo.pSetLayouts = !setLayoutHandles.empty() ? setLayoutHandles.data() : nullptr;
        layoutInfo.setLayoutCount = uint32_t(setLayoutHandles.size());

        VkResult res = vkCreatePipelineLayout(device->vk, &layoutInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreatePipelineLayout failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanPipelineLayout::~VulkanPipelineLayout() {
        if (vk != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device->vk, vk, nullptr);
        }

        for (VulkanDescriptorSetLayout *setLayout : descriptorSetLayouts) {
            delete setLayout;
        }
    }

    // VulkanShader

    VulkanShader::VulkanShader(VulkanDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) {
        assert(device != nullptr);
        assert(data != nullptr);
        assert(size > 0);
        assert(format != RenderShaderFormat::UNKNOWN);
        assert(format == RenderShaderFormat::SPIRV);

        this->device = device;
        this->format = format;
        this->entryPointName = (entryPointName != nullptr) ? std::string(entryPointName) : std::string();

        VkShaderModuleCreateInfo shaderInfo = {};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.pCode = reinterpret_cast<const uint32_t *>(data);
        shaderInfo.codeSize = size;
        VkResult res = vkCreateShaderModule(device->vk, &shaderInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateShaderModule failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanShader::~VulkanShader() {
        if (vk != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device->vk, vk, nullptr);
        }
    }

    void VulkanShader::setName(const std::string &name) {
        setObjectName(device->vk, VK_OBJECT_TYPE_SHADER_MODULE, uint64_t(vk), name);
    }

    // VulkanSampler

    VulkanSampler::VulkanSampler(VulkanDevice *device, const RenderSamplerDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = toVk(desc.minFilter);
        samplerInfo.magFilter = toVk(desc.magFilter);
        samplerInfo.mipmapMode = toVk(desc.mipmapMode);
        samplerInfo.addressModeU = toVk(desc.addressU, device->capabilities.samplerMirrorClampToEdge);
        samplerInfo.addressModeV = toVk(desc.addressV, device->capabilities.samplerMirrorClampToEdge);
        samplerInfo.addressModeW = toVk(desc.addressW, device->capabilities.samplerMirrorClampToEdge);
        samplerInfo.mipLodBias = desc.mipLODBias;
        samplerInfo.anisotropyEnable = desc.anisotropyEnabled;
        samplerInfo.maxAnisotropy = float(desc.maxAnisotropy);
        samplerInfo.compareEnable = desc.comparisonEnabled;
        samplerInfo.compareOp = toVk(desc.comparisonFunc);
        samplerInfo.minLod = desc.minLOD;
        samplerInfo.maxLod = desc.maxLOD;
        samplerInfo.borderColor = toVk(desc.borderColor);
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        VkResult res = vkCreateSampler(device->vk, &samplerInfo, nullptr, &vk);
#if defined(__ANDROID__)
        if (res != VK_SUCCESS) vk = VK_NULL_HANDLE;
        device->descriptorResources.track(this, VK_OBJECT_TYPE_SAMPLER, uint64_t(vk));
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateSampler failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanSampler::~VulkanSampler() {
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        device->descriptorResources.erase(this);
#endif
        if (vk != VK_NULL_HANDLE) {
            vkDestroySampler(device->vk, vk, nullptr);
        }
    }

    // VulkanPipeline

    VulkanPipeline::VulkanPipeline(VulkanDevice *device, Type type) {
        assert(device != nullptr);
        assert(type != Type::Unknown);

        this->device = device;
        this->type = type;
    }

    VulkanPipeline::~VulkanPipeline() { }

    // VulkanComputePipeline

    VulkanComputePipeline::VulkanComputePipeline(VulkanDevice *device, const RenderComputePipelineDesc &desc) : VulkanPipeline(device, Type::Compute) {
        assert(desc.computeShader != nullptr);
        assert(desc.pipelineLayout != nullptr);
        assert((desc.threadGroupSizeX > 0) && (desc.threadGroupSizeY > 0) && (desc.threadGroupSizeZ > 0));

        std::vector<VkSpecializationMapEntry> specEntries(desc.specConstantsCount);
        std::vector<uint32_t> specData(desc.specConstantsCount);
        VkSpecializationInfo specInfo = {};
        fillSpecInfo(desc.specConstants, desc.specConstantsCount, specInfo, specEntries.data(), specData.data());

        const VulkanShader *computeShader = static_cast<const VulkanShader *>(desc.computeShader);
        VkPipelineShaderStageCreateInfo stageInfo = {};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = computeShader->vk;
        stageInfo.pName = computeShader->entryPointName.c_str();
        stageInfo.pSpecializationInfo = (specInfo.mapEntryCount > 0) ? &specInfo : nullptr;

        const VulkanPipelineLayout *pipelineLayout = static_cast<const VulkanPipelineLayout *>(desc.pipelineLayout);
        VkComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout->vk;
        pipelineInfo.stage = stageInfo;

        VkResult res = vkCreateComputePipelines(device->vk, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateComputePipelines failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanComputePipeline::~VulkanComputePipeline() {
        if (vk != VK_NULL_HANDLE) {
            vkDestroyPipeline(device->vk, vk, nullptr);
        }
    }

    void VulkanComputePipeline::setName(const std::string &name) {
        setObjectName(device->vk, VK_OBJECT_TYPE_PIPELINE, uint64_t(vk), name);
    }

    RenderPipelineProgram VulkanComputePipeline::getProgram(const std::string &name) const {
        assert(false && "Compute pipelines can't retrieve shader programs.");
        return RenderPipelineProgram();
    }

    // VulkanGraphicsPipeline

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanDevice *device, const RenderGraphicsPipelineDesc &desc) : VulkanPipeline(device, Type::Graphics) {
        assert(desc.pipelineLayout != nullptr);

#if defined(__ANDROID__)
        // Image-usage support does not establish blend support. This also checks
        // owned BGRA UI targets independently of the Android surface format list.
        for (uint32_t i = 0; i < desc.renderTargetCount; ++i) {
            if (!desc.renderTargetBlend[i].blendEnabled) continue;
            const VkFormat format = toVk(desc.renderTargetFormat[i]);
            VkFormatProperties properties = {};
            vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &properties);
            const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
            if ((properties.optimalTilingFeatures & required) != required) {
                char message[256];
                std::snprintf(message, sizeof(message),
                    "Graphics pipeline rejected: reason=attachment-blend-unsupported attachment=%u format=%u optimal=0x%X required=0x%X",
                    i, unsigned(format), properties.optimalTilingFeatures, required);
                tooie_android_debug_log(message);
                std::fprintf(stderr, "%s\n", message);
                std::_Exit(EXIT_FAILURE);
            }
        }
#endif
        thread_local std::vector<VkPipelineShaderStageCreateInfo> stages;
        stages.clear();

        std::vector<VkSpecializationMapEntry> specEntries(desc.specConstantsCount);
        std::vector<uint32_t> specData(desc.specConstantsCount);
        VkSpecializationInfo specInfo = {};
        fillSpecInfo(desc.specConstants, desc.specConstantsCount, specInfo, specEntries.data(), specData.data());

        const VkSpecializationInfo *pSpecInfo = (specInfo.mapEntryCount > 0) ? &specInfo : nullptr;
        if (desc.vertexShader != nullptr) {
            const VulkanShader *vertexShader = static_cast<const VulkanShader *>(desc.vertexShader);
            VkPipelineShaderStageCreateInfo stageInfo = {};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            stageInfo.module = vertexShader->vk;
            stageInfo.pName = vertexShader->entryPointName.c_str();
            stageInfo.pSpecializationInfo = pSpecInfo;
            stages.emplace_back(stageInfo);
        }

        if (desc.geometryShader != nullptr) {
            const VulkanShader *geometryShader = static_cast<const VulkanShader *>(desc.geometryShader);
            VkPipelineShaderStageCreateInfo stageInfo = {};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            stageInfo.module = geometryShader->vk;
            stageInfo.pName = geometryShader->entryPointName.c_str();
            stageInfo.pSpecializationInfo = pSpecInfo;
            stages.emplace_back(stageInfo);
        }

        if (desc.pixelShader != nullptr) {
            const VulkanShader *pixelShader = static_cast<const VulkanShader *>(desc.pixelShader);
            VkPipelineShaderStageCreateInfo stageInfo = {};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stageInfo.module = pixelShader->vk;
            stageInfo.pName = pixelShader->entryPointName.c_str();
            stageInfo.pSpecializationInfo = pSpecInfo;
            stages.emplace_back(stageInfo);
        }

        thread_local std::vector<VkVertexInputBindingDescription> vertexBindings;
        thread_local std::vector<VkVertexInputAttributeDescription> vertexAttributes;
        vertexBindings.clear();
        vertexAttributes.clear();

        for (uint32_t i = 0; i < desc.inputSlotsCount; i++) {
            const RenderInputSlot &inputSlot = desc.inputSlots[i];
            VkVertexInputBindingDescription binding = {};
            binding.binding = inputSlot.index;
            binding.stride = inputSlot.stride;
            binding.inputRate = toVk(inputSlot.classification);
            vertexBindings.emplace_back(binding);
        }

        for (uint32_t i = 0; i < desc.inputElementsCount; i++) {
            const RenderInputElement &inputElement = desc.inputElements[i];
            VkVertexInputAttributeDescription attribute = {};
            attribute.location = inputElement.location;
            attribute.binding = inputElement.slotIndex;
            attribute.format = toVk(inputElement.format);
            attribute.offset = inputElement.alignedByteOffset;
            vertexAttributes.emplace_back(attribute);
        }

        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.pVertexBindingDescriptions = !vertexBindings.empty() ? vertexBindings.data() : nullptr;
        vertexInput.vertexBindingDescriptionCount = uint32_t(vertexBindings.size());
        vertexInput.pVertexAttributeDescriptions = !vertexAttributes.empty() ? vertexAttributes.data() : nullptr;
        vertexInput.vertexAttributeDescriptionCount = uint32_t(vertexAttributes.size());

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = toVk(desc.primitiveTopology);
        if (desc.primitiveTopology == RenderPrimitiveTopology::LINE_STRIP || desc.primitiveTopology == RenderPrimitiveTopology::TRIANGLE_STRIP) {
            inputAssembly.primitiveRestartEnable = VK_TRUE;
        }

        uint32_t renderTargetCount = desc.renderTargetCount;
        if (renderTargetCount == 0 && desc.depthTargetFormat != RenderFormat::UNKNOWN) {
            renderTargetCount = 1;
        }

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = renderTargetCount;
        viewportState.scissorCount = renderTargetCount;

        VkPipelineRasterizationStateCreateInfo rasterization = {};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.depthClampEnable = !desc.depthClipEnabled;
        rasterization.rasterizerDiscardEnable = VK_FALSE;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.lineWidth = 1.0f;
        rasterization.cullMode = toVk(desc.cullMode);
        rasterization.frontFace = toVk(desc.frontFace);

        if (desc.dynamicDepthBiasEnabled) {
            rasterization.depthBiasEnable = true;
        }
        else if ((desc.depthBias != 0) || (desc.depthBiasClamp != 0.0f) || (desc.slopeScaledDepthBias != 0.0f)) {
            rasterization.depthBiasEnable = true;
            rasterization.depthBiasConstantFactor = float(desc.depthBias);
            rasterization.depthBiasClamp = desc.depthBiasClamp;
            rasterization.depthBiasSlopeFactor = desc.slopeScaledDepthBias;
        }

        thread_local std::vector<VkSampleLocationEXT> sampleLocationVector;
        VkSampleLocationsInfoEXT sampleLocationsInfo = {};
        VkPipelineSampleLocationsStateCreateInfoEXT sampleLocations = {};
        const void *multisamplingNext = nullptr;
        if (desc.multisampling.sampleLocationsEnabled) {
            const float *coordinateRange = device->sampleLocationProperties.sampleLocationCoordinateRange;
            const float coordinateBase = coordinateRange[0];
            const float coordinateSpace = (coordinateRange[1] - coordinateRange[0]) / 15.0f;
            sampleLocationVector.resize(desc.multisampling.sampleCount);
            for (uint32_t i = 0; i < desc.multisampling.sampleCount; i++) {
                const RenderMultisamplingLocation &location = desc.multisampling.sampleLocations[i];
                sampleLocationVector[i].x = coordinateBase + (location.x + 8) * coordinateSpace;
                sampleLocationVector[i].y = coordinateBase + (location.y + 8) * coordinateSpace;
            }

            sampleLocationsInfo.sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;
            sampleLocationsInfo.sampleLocationsPerPixel = VkSampleCountFlagBits(desc.multisampling.sampleCount);
            sampleLocationsInfo.sampleLocationGridSize.width = 1;
            sampleLocationsInfo.sampleLocationGridSize.height = 1;
            sampleLocationsInfo.sampleLocationsCount = uint32_t(sampleLocationVector.size());
            sampleLocationsInfo.pSampleLocations = sampleLocationVector.data();

            sampleLocations.sType = VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT;
            sampleLocations.sampleLocationsEnable = true;
            sampleLocations.sampleLocationsInfo = sampleLocationsInfo;
            multisamplingNext = &sampleLocations;
        }

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.pNext = multisamplingNext;
        multisampling.rasterizationSamples = VkSampleCountFlagBits(desc.multisampling.sampleCount);
        multisampling.alphaToCoverageEnable = desc.alphaToCoverageEnabled;

        thread_local std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
        colorBlendAttachments.clear();

        for (uint32_t i = 0; i < desc.renderTargetCount; i++) {
            VkPipelineColorBlendAttachmentState attachment = {};
            const RenderBlendDesc &blendDesc = desc.renderTargetBlend[i];
            attachment.blendEnable = blendDesc.blendEnabled;
            attachment.srcColorBlendFactor = toVk(blendDesc.srcBlend);
            attachment.dstColorBlendFactor = toVk(blendDesc.dstBlend);
            attachment.colorBlendOp = toVk(blendDesc.blendOp);
            attachment.srcAlphaBlendFactor = toVk(blendDesc.srcBlendAlpha);
            attachment.dstAlphaBlendFactor = toVk(blendDesc.dstBlendAlpha);
            attachment.alphaBlendOp = toVk(blendDesc.blendOpAlpha);
            attachment.colorWriteMask = blendDesc.renderTargetWriteMask;
            colorBlendAttachments.emplace_back(attachment);
        }

        VkPipelineColorBlendStateCreateInfo colorBlend = {};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.logicOpEnable = desc.logicOpEnabled;
        colorBlend.logicOp = toVk(desc.logicOp);
        colorBlend.pAttachments = !colorBlendAttachments.empty() ? colorBlendAttachments.data() : nullptr;
        colorBlend.attachmentCount = uint32_t(colorBlendAttachments.size());
        
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = desc.depthEnabled;
        depthStencil.depthWriteEnable = desc.depthWriteEnabled;
        depthStencil.depthCompareOp = toVk(desc.depthFunction);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = desc.stencilEnabled;
        depthStencil.front.passOp = toVk(desc.stencilFrontFace.passOp);
        depthStencil.front.failOp = toVk(desc.stencilFrontFace.failOp);
        depthStencil.front.depthFailOp = toVk(desc.stencilFrontFace.depthFailOp);
        depthStencil.front.compareOp = toVk(desc.stencilFrontFace.compareFunction);
        depthStencil.front.compareMask = desc.stencilReadMask;
        depthStencil.front.writeMask = desc.stencilWriteMask;
        depthStencil.front.reference = desc.stencilReference;
        depthStencil.back.passOp = toVk(desc.stencilBackFace.passOp);
        depthStencil.back.failOp = toVk(desc.stencilBackFace.failOp);
        depthStencil.back.depthFailOp = toVk(desc.stencilBackFace.depthFailOp);
        depthStencil.back.compareOp = toVk(desc.stencilBackFace.compareFunction);
        depthStencil.back.compareMask = desc.stencilReadMask;
        depthStencil.back.writeMask = desc.stencilWriteMask;
        depthStencil.back.reference = desc.stencilReference;

        thread_local std::vector<VkDynamicState> dynamicStates;
        dynamicStates.clear();
        dynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
        dynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);

        if (desc.dynamicDepthBiasEnabled) {
            dynamicStates.emplace_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
        }

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pDynamicStates = dynamicStates.data();
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());

        thread_local std::vector<VkFormat> renderTargetFormats;
        renderTargetFormats.resize(desc.renderTargetCount);
        for (uint32_t i = 0; i < desc.renderTargetCount; i++) {
            renderTargetFormats[i] = toVk(desc.renderTargetFormat[i]);
        }

        renderPass = createRenderPass(device, renderTargetFormats.data(), desc.renderTargetCount, toVk(desc.depthTargetFormat), VkSampleCountFlagBits(desc.multisampling.sampleCount));
        if (renderPass == VK_NULL_HANDLE) {
            return;
        }

        const VulkanPipelineLayout *pipelineLayout = static_cast<const VulkanPipelineLayout *>(desc.pipelineLayout);
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pStages = stages.data();
        pipelineInfo.stageCount = uint32_t(stages.size());
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout->vk;
        pipelineInfo.renderPass = renderPass;

        VkResult res = vkCreateGraphicsPipelines(device->vk, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateGraphicsPipelines failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
        if (vk != VK_NULL_HANDLE) {
            vkDestroyPipeline(device->vk, vk, nullptr);
        }

        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device->vk, renderPass, nullptr);
        }
    }

    void VulkanGraphicsPipeline::setName(const std::string &name) {
        setObjectName(device->vk, VK_OBJECT_TYPE_PIPELINE, uint64_t(vk), name);
    }

    RenderPipelineProgram VulkanGraphicsPipeline::getProgram(const std::string &name) const {
        assert(false && "Graphics pipelines can't retrieve shader programs.");
        return RenderPipelineProgram();
    }

    VkRenderPass VulkanGraphicsPipeline::createRenderPass(VulkanDevice *device, const VkFormat *renderTargetFormat, uint32_t renderTargetCount, VkFormat depthTargetFormat, VkSampleCountFlagBits sampleCount) {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkSubpassDescription subpass = {};
        VkAttachmentReference depthReference = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        thread_local std::vector<VkAttachmentDescription> attachments;
        thread_local std::vector<VkAttachmentReference> colorReferences;
        attachments.clear();
        colorReferences.clear();
        for (uint32_t i = 0; i < renderTargetCount; i++) {
            VkAttachmentReference reference = {};
            reference.attachment = uint32_t(attachments.size());
            reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorReferences.emplace_back(reference);

            VkAttachmentDescription attachment = {};
            attachment.format = renderTargetFormat[i];
            attachment.samples = sampleCount;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments.emplace_back(attachment);
        }

        subpass.pColorAttachments = !colorReferences.empty() ? colorReferences.data() : nullptr;
        subpass.colorAttachmentCount = uint32_t(colorReferences.size());

        if (depthTargetFormat != VK_FORMAT_UNDEFINED) {
            depthReference.attachment = uint32_t(attachments.size());
            depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            subpass.pDepthStencilAttachment = &depthReference;

            VkAttachmentDescription attachment = {};
            attachment.format = depthTargetFormat;
            attachment.samples = sampleCount;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = attachment.loadOp;
            attachment.stencilStoreOp = attachment.storeOp;
            attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.emplace_back(attachment);
        }

        VkRenderPassCreateInfo passInfo = {};
        passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        passInfo.pAttachments = !attachments.empty() ? attachments.data() : nullptr;
        passInfo.attachmentCount = uint32_t(attachments.size());
        passInfo.pSubpasses = &subpass;
        passInfo.subpassCount = 1;

        VkResult res = vkCreateRenderPass(device->vk, &passInfo, nullptr, &renderPass);
        if (res == VK_SUCCESS) {
            return renderPass;
        }
        else {
            fprintf(stderr, "vkCreateRenderPass failed with error code 0x%X.\n", res);
            return VK_NULL_HANDLE;
        }
    }

    // VulkanRaytracingPipeline

    VulkanRaytracingPipeline::VulkanRaytracingPipeline(VulkanDevice *device, const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline) : VulkanPipeline(device, VulkanPipeline::Type::Raytracing) {
        assert(desc.pipelineLayout != nullptr);
        assert(!desc.stateUpdateEnabled && "State updates are not supported.");

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
        std::unordered_map<std::string, uint32_t> shaderIndices;

        // Prepare all the vectors for the spec constants beforehand so they're not re-allocated.
        std::vector<VkSpecializationMapEntry> specEntries;
        std::vector<uint32_t> specData;
        std::vector<VkSpecializationInfo> specInfo;
        for (uint32_t i = 0; i < desc.librariesCount; i++) {
            const RenderRaytracingPipelineLibrary &library = desc.libraries[i];
            for (uint32_t j = 0; j < library.symbolsCount; j++) {
                const RenderRaytracingPipelineLibrarySymbol &symbol = library.symbols[j];
                if (symbol.specConstantsCount == 0) {
                    continue;
                }

                for (uint32_t i = 0; i < symbol.specConstantsCount; i++) {
                    specEntries.emplace_back();
                    specData.emplace_back();
                }

                specInfo.emplace_back();
            }
        }

        uint32_t specConstantIndex = 0;
        uint32_t specConstantCursor = 0;
        for (uint32_t i = 0; i < desc.librariesCount; i++) {
            const RenderRaytracingPipelineLibrary &library = desc.libraries[i];
            assert(library.shader != nullptr);

            const VulkanShader *interfaceShader = static_cast<const VulkanShader *>(library.shader);
            for (uint32_t j = 0; j < library.symbolsCount; j++) {
                const RenderRaytracingPipelineLibrarySymbol &symbol = library.symbols[j];
                const bool isRaygen = (symbol.type == RenderRaytracingPipelineLibrarySymbolType::RAYGEN);
                const bool isMiss = (symbol.type == RenderRaytracingPipelineLibrarySymbolType::MISS);
                const uint32_t shaderStageIndex = uint32_t(shaderStages.size());
                const char *exportName = (symbol.exportName != nullptr) ? symbol.exportName : symbol.importName;
                if (isRaygen || isMiss) {
                    VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
                    groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                    groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                    groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
                    groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
                    groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
                    groupInfo.generalShader = shaderStageIndex;
                    nameProgramMap[std::string(exportName)] = uint32_t(shaderGroups.size());
                    shaderGroups.emplace_back(groupInfo);
                }

                VkPipelineShaderStageCreateInfo stageInfo = {};
                stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stageInfo.pName = symbol.importName;
                stageInfo.module = interfaceShader->vk;
                stageInfo.stage = toStage(symbol.type);

                if (symbol.specConstantsCount > 0) {
                    stageInfo.pSpecializationInfo = &specInfo[specConstantIndex];
                    fillSpecInfo(symbol.specConstants, symbol.specConstantsCount, specInfo[specConstantIndex], &specEntries[specConstantCursor], &specData[specConstantCursor]);
                    specConstantCursor += symbol.specConstantsCount;
                    specConstantIndex++;
                }

                shaderIndices[std::string(exportName)] = uint32_t(shaderStages.size());
                shaderStages.emplace_back(stageInfo);
            }
        }

        for (uint32_t i = 0; i < desc.hitGroupsCount; i++) {
            auto getShaderIndex = [&](const char *name) {
                if (name != nullptr) {
                    auto it = shaderIndices.find(std::string(name));
                    assert(it != shaderIndices.end());
                    return it->second;
                }
                else {
                    return uint32_t(VK_SHADER_UNUSED_KHR);
                }
            };

            const RenderRaytracingPipelineHitGroup &hitGroup = desc.hitGroups[i];
            VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
            groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
            groupInfo.closestHitShader = getShaderIndex(hitGroup.closestHitName);
            groupInfo.anyHitShader = getShaderIndex(hitGroup.anyHitName);
            groupInfo.intersectionShader = getShaderIndex(hitGroup.intersectionName);
            nameProgramMap[std::string(hitGroup.hitGroupName)] = uint32_t(shaderGroups.size());
            shaderGroups.emplace_back(groupInfo);
        }

        VkRayTracingPipelineInterfaceCreateInfoKHR interfaceInfo = {};
        interfaceInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR;
        interfaceInfo.maxPipelineRayPayloadSize = desc.maxPayloadSize;
        interfaceInfo.maxPipelineRayHitAttributeSize = desc.maxAttributeSize;

        const VulkanPipelineLayout *pipelineLayout = static_cast<const VulkanPipelineLayout *>(desc.pipelineLayout);
        VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pGroups = shaderGroups.data();
        pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
        pipelineInfo.maxPipelineRayRecursionDepth = desc.maxRecursionDepth;
        pipelineInfo.layout = pipelineLayout->vk;

        this->descriptorSetCount = uint32_t(pipelineLayout->descriptorSetLayouts.size());

        VkResult res = vkCreateRayTracingPipelinesKHR(device->vk, nullptr, nullptr, 1, &pipelineInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateRayTracingPipelinesKHR failed with error code 0x%X.\n", res);
            return;
        }

        groupCount = pipelineInfo.groupCount;
    }
    
    VulkanRaytracingPipeline::~VulkanRaytracingPipeline() {
        if (vk != VK_NULL_HANDLE) {
            vkDestroyPipeline(device->vk, vk, nullptr);
        }
    }

    void VulkanRaytracingPipeline::setName(const std::string &name) {
        setObjectName(device->vk, VK_OBJECT_TYPE_PIPELINE, uint64_t(vk), name);
    }

    RenderPipelineProgram VulkanRaytracingPipeline::getProgram(const std::string &name) const {
        auto it = nameProgramMap.find(name);
        assert((it != nameProgramMap.end()) && "Program must exist in the PSO.");
        return it->second;
    }

    // VulkanDescriptorSet

    VulkanDescriptorSet::VulkanDescriptorSet(VulkanDevice *device, const RenderDescriptorSetDesc &desc) {
        assert(device != nullptr);

        this->device = device;
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        if (!device->vk) rejectDescriptor(UINT32_MAX, "null-device-handle");
        for (uint32_t i = 0; desc.descriptorRanges && i < desc.descriptorRangesCount; ++i) {
            const auto &range = desc.descriptorRanges[i];
            if (!range.count || toVk(range.type) == VK_DESCRIPTOR_TYPE_MAX_ENUM)
                rejectDescriptor(UINT32_MAX, "invalid-descriptor-range-type-or-count");
        }
        if ((desc.descriptorRangesCount && !desc.descriptorRanges) ||
            (desc.lastRangeIsBoundless && (!desc.descriptorRangesCount ||
                std::max(desc.boundlessRangeSize, 1u) > desc.descriptorRanges[desc.descriptorRangesCount - 1].count)))
            rejectDescriptor(UINT32_MAX, "invalid-descriptor-range-or-variable-count");
#endif
#if defined(__ANDROID__)
        std::unordered_map<VkDescriptorType, uint32_t> typeCounts;
#else
        thread_local std::unordered_map<VkDescriptorType, uint32_t> typeCounts;
#endif
        typeCounts.clear();
        
        uint32_t boundlessRangeSize = 0;
        uint32_t rangeCount = desc.descriptorRangesCount;
        if (desc.lastRangeIsBoundless) {
            assert((desc.descriptorRangesCount > 0) && "There must be at least one descriptor set to define the last range as boundless.");

            // Ensure at least one entry is created for boundless ranges.
            boundlessRangeSize = std::max(desc.boundlessRangeSize, 1U);

            const RenderDescriptorRange &lastDescriptorRange = desc.descriptorRanges[desc.descriptorRangesCount - 1];
            typeCounts[toVk(lastDescriptorRange.type)] += boundlessRangeSize;
            rangeCount--;
        }

        for (uint32_t i = 0; i < rangeCount; i++) {
            const RenderDescriptorRange &descriptorRange = desc.descriptorRanges[i];
            typeCounts[toVk(descriptorRange.type)] += descriptorRange.count;
        }

        setLayout = new VulkanDescriptorSetLayout(device, desc);
#if defined(__ANDROID__)
        initializationFailure = "descriptor-layout-creation-failed";
        if (!setLayout->vk || setLayout->creationResult != VK_SUCCESS) rejectDescriptor(UINT32_MAX, initializationFailure, setLayout->creationResult);
        for (const auto &binding : setLayout->setBindings) allocatedCounts.push_back(binding.descriptorCount);
        if (desc.lastRangeIsBoundless) allocatedCounts.back() = boundlessRangeSize;
        // Size the pool from the actual Vulkan layout and requested variable
        // count, including immutable samplers. Never reserve an arbitrary slack
        // multiplier or substitute the 8192-entry upper bound for a small set.
        typeCounts.clear();
        for (size_t i = 0; i < allocatedCounts.size(); ++i) typeCounts[setLayout->setBindings[i].descriptorType] += allocatedCounts[i];
        char allocationRecord[512];
        for (size_t i = 0; i < allocatedCounts.size(); ++i) {
            const auto &binding = setLayout->setBindings[i];
            std::snprintf(allocationRecord, sizeof(allocationRecord), "Descriptor allocation binding: pid=%d tid=%d ctx=%s layout=%llx binding=%u type=%u declared=%u requested=%u immutable=%u flags=0x%X",
                int(getpid()), int(gettid()), tooie::android::descriptorContext(), (unsigned long long)setLayout->vk, binding.binding,
                unsigned(binding.descriptorType), binding.descriptorCount, allocatedCounts[i], unsigned(binding.pImmutableSamplers != nullptr), setLayout->bindingFlags[i]);
            tooie_android_debug_log(allocationRecord);
        }
#endif
        VkResult poolResult = VK_NOT_READY;
        descriptorPool = createDescriptorPool(device, typeCounts, desc.lastRangeIsBoundless, &poolResult);
        if (descriptorPool == VK_NULL_HANDLE) {
#if defined(__ANDROID__)
            initializationFailure = "descriptor-pool-creation-failed";
            rejectDescriptor(UINT32_MAX, initializationFailure, poolResult);
#endif
            return;
        }

        VkDescriptorSetAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = descriptorPool;
        allocateInfo.pSetLayouts = &setLayout->vk;
        allocateInfo.descriptorSetCount = 1;

        VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo = {};
        if (desc.lastRangeIsBoundless) {
            countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            countInfo.pDescriptorCounts = &boundlessRangeSize;
            countInfo.descriptorSetCount = 1;
            allocateInfo.pNext = &countInfo;
        }

#if defined(__ANDROID__)
        const auto *actualCount = static_cast<const VkDescriptorSetVariableDescriptorCountAllocateInfo *>(allocateInfo.pNext);
        if (allocateInfo.descriptorPool != descriptorPool || allocateInfo.descriptorSetCount != 1 || allocateInfo.pSetLayouts != &setLayout->vk ||
            (desc.lastRangeIsBoundless && (actualCount != &countInfo || actualCount->pNext || actualCount->descriptorSetCount != 1 ||
                actualCount->sType != VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO || actualCount->pDescriptorCounts != &boundlessRangeSize ||
                *actualCount->pDescriptorCounts != allocatedCounts.back())) || (!desc.lastRangeIsBoundless && actualCount))
            rejectDescriptor(UINT32_MAX, "invalid-allocation-arguments");
        std::snprintf(allocationRecord, sizeof(allocationRecord), "Descriptor allocation ABI: pid=%d tid=%d pointerBytes=%zu infoBytes=%zu nextOffset=%zu layoutsOffset=%zu variableBytes=%zu countsOffset=%zu",
            int(getpid()), int(gettid()), sizeof(void *), sizeof(allocateInfo), offsetof(VkDescriptorSetAllocateInfo, pNext),
            offsetof(VkDescriptorSetAllocateInfo, pSetLayouts), sizeof(countInfo), offsetof(VkDescriptorSetVariableDescriptorCountAllocateInfo, pDescriptorCounts));
        tooie_android_debug_log(allocationRecord);
        std::snprintf(allocationRecord, sizeof(allocationRecord), "Descriptor allocation arguments: pid=%d tid=%d device=%llx info=%p pool=%llx layouts=%p layout=%llx next=%p countArray=%p actualCount=%u",
            int(getpid()), int(gettid()), (unsigned long long)device->vk, static_cast<const void *>(&allocateInfo), (unsigned long long)allocateInfo.descriptorPool,
            static_cast<const void *>(allocateInfo.pSetLayouts), (unsigned long long)*allocateInfo.pSetLayouts, allocateInfo.pNext,
            actualCount ? static_cast<const void *>(actualCount->pDescriptorCounts) : nullptr, actualCount ? *actualCount->pDescriptorCounts : 0);
        tooie_android_debug_log(allocationRecord);
        std::snprintf(allocationRecord, sizeof(allocationRecord), "Descriptor allocation begin: pid=%d tid=%d ctx=%s pool=%llx layout=%llx layoutFlags=0x%X maxSets=1 sets=1 variable=%u variableCount=%u countStructType=%u countStructSets=%u",
            int(getpid()), int(gettid()), tooie::android::descriptorContext(), (unsigned long long)descriptorPool, (unsigned long long)setLayout->vk,
            setLayout->creationFlags, unsigned(desc.lastRangeIsBoundless), boundlessRangeSize, unsigned(countInfo.sType), countInfo.descriptorSetCount);
        tooie_android_debug_log(allocationRecord);
#endif
        VkResult res = vkAllocateDescriptorSets(device->vk, &allocateInfo, &vk);
#if defined(__ANDROID__)
        initializationFailure = "descriptor-set-allocation-failed";
        std::snprintf(allocationRecord, sizeof(allocationRecord), "Descriptor allocation returned: pid=%d tid=%d ctx=%s pool=%llx layout=%llx result=%d returnedSet=%llx",
            int(getpid()), int(gettid()), tooie::android::descriptorContext(), (unsigned long long)descriptorPool, (unsigned long long)setLayout->vk, int(res), (unsigned long long)vk);
        tooie_android_debug_log(allocationRecord);
        if (res != VK_SUCCESS) { vk = VK_NULL_HANDLE; rejectDescriptor(UINT32_MAX, initializationFailure, res); }
        if (!vk) { initializationFailure = "descriptor-set-allocation-null-handle"; rejectDescriptor(UINT32_MAX, initializationFailure, res); }
        device->descriptorResources.track(this, VK_OBJECT_TYPE_DESCRIPTOR_SET, uint64_t(vk), setLayout);
        initializationFailure = "none";
        // Layout shape is diagnostic evidence, especially for the first real
        // RSP/typed-buffer set after a launcher with only texture descriptors.
        uint32_t typed = typeCounts[VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER] + typeCounts[VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER];
        thread_local bool loggedTyped = false;
        thread_local unsigned loggedSets = 0;
        if (loggedSets++ < 8 || (typed && !loggedTyped)) {
            char record[384];
            std::snprintf(record, sizeof(record), "Descriptor set allocated: pid=%d tid=%d set=%llx layout=%llx bindings=%u typed=%u sampled=%u storageImages=%u buffers=%u variable=%u allocatedLast=%u",
                int(getpid()), int(gettid()), (unsigned long long)vk, (unsigned long long)setLayout->vk, unsigned(allocatedCounts.size()), typed,
                typeCounts[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE], typeCounts[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE],
                typeCounts[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER] + typeCounts[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER], unsigned(desc.lastRangeIsBoundless),
                allocatedCounts.empty() ? 0 : allocatedCounts.back());
            tooie_android_debug_log(record);
            std::fprintf(stderr, "%s\n", record);
        }
        loggedTyped |= typed != 0;
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkAllocateDescriptorSets failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanDescriptorSet::~VulkanDescriptorSet() {
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        device->descriptorResources.erase(this);
#endif
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device->vk, descriptorPool, nullptr);
        }

        delete setLayout;
    }
    
#if defined(__ANDROID__)
    [[noreturn]] void VulkanDescriptorSet::rejectDescriptor(uint32_t index, const char *reason, VkResult result) const {
        uint32_t binding = UINT32_MAX, count = 0, type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        if (setLayout && index < setLayout->descriptorBindingIndices.size()) {
            const uint32_t b = setLayout->descriptorBindingIndices[index];
            if (b < setLayout->setBindings.size()) {
                binding = setLayout->setBindings[b].binding;
                type = setLayout->setBindings[b].descriptorType;
                if (b < allocatedCounts.size()) count = allocatedCounts[b];
            }
        }
        char record[768];
        std::snprintf(record, sizeof(record),
            "Descriptor rejected: reason=%s result=%d op=%s ctx=%s pid=%d tid=%d set=%llx pool=%llx setLayout=%llx init=%s index=%u binding=%u type=%u allocated=%u "
            "resource=%llx view=%llx offset=%llu range=%llu format=%u usage=0x%X flags=0x%X samples=%u layout=%u",
            reason, int(result), writeRecord.operation, tooie::android::descriptorContext(), int(getpid()), int(gettid()), (unsigned long long)vk, (unsigned long long)descriptorPool,
            (unsigned long long)(setLayout ? setLayout->vk : VK_NULL_HANDLE), initializationFailure,
            index, binding, type, count, (unsigned long long)writeRecord.resource, (unsigned long long)writeRecord.view,
            (unsigned long long)writeRecord.offset, (unsigned long long)writeRecord.range,
            writeRecord.format, writeRecord.usage, writeRecord.flags, writeRecord.samples, writeRecord.layout);
        tooie_android_debug_log(record);
        std::fprintf(stderr, "%s\n", record);
        std::_Exit(EXIT_FAILURE);
    }

    void VulkanDescriptorSet::validateDescriptorIndex(uint32_t index) const {
        if (!device->vk) rejectDescriptor(index, "null-device-handle");
        if (!vk || !descriptorPool || !setLayout || !setLayout->vk ||
            !device->descriptorResources.live(this, VK_OBJECT_TYPE_DESCRIPTOR_SET))
            rejectDescriptor(index, "descriptor-set-not-initialized-or-live");
        if (device->descriptorResources.objects.at(this).handle != uint64_t(vk)) rejectDescriptor(index, "descriptor-set-handle-mismatch");
        for (const auto &sampler : setLayout->immutableSamplerResources) {
            if (!device->descriptorResources.live(sampler.first, VK_OBJECT_TYPE_SAMPLER) ||
                device->descriptorResources.objects.at(sampler.first).generation != sampler.second)
                rejectDescriptor(index, "immutable-sampler-not-live");
        }
        if (index >= setLayout->descriptorBindingIndices.size() || index >= setLayout->descriptorIndexBases.size())
            rejectDescriptor(index, "descriptor-index-out-of-bounds");
        const uint32_t binding = setLayout->descriptorBindingIndices[index];
        if (binding >= setLayout->setBindings.size() || binding >= allocatedCounts.size() ||
            index < setLayout->descriptorIndexBases[index]) rejectDescriptor(index, "invalid-binding-metadata");
        if (index - setLayout->descriptorIndexBases[index] >= allocatedCounts[binding])
            rejectDescriptor(index, "descriptor-array-element-not-allocated");
    }
#endif

    void VulkanDescriptorSet::setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) {
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        writeRecord = {}; writeRecord.operation = "buffer";
        validateDescriptorIndex(descriptorIndex);
        if (!buffer) rejectDescriptor(descriptorIndex, "null-buffer-object");
        if (!device->descriptorResources.live(buffer, VK_OBJECT_TYPE_BUFFER)) rejectDescriptor(descriptorIndex, "buffer-not-live-or-wrong-device");
#else
        if (buffer == nullptr) return;
#endif
        const VulkanBuffer *interfaceBuffer = static_cast<const VulkanBuffer *>(buffer);
        const VkBufferView *bufferView = nullptr;
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = interfaceBuffer->vk;
        bufferInfo.range = (bufferSize > 0) ? bufferSize : interfaceBuffer->desc.size;

#if defined(__ANDROID__)
        writeRecord.resource = uint64_t(interfaceBuffer->vk);
        writeRecord.usage = interfaceBuffer->bufferUsage;
        writeRecord.flags = interfaceBuffer->desc.flags;
        if (device->descriptorResources.objects.at(buffer).handle != uint64_t(interfaceBuffer->vk)) rejectDescriptor(descriptorIndex, "buffer-handle-mismatch");
        if (!interfaceBuffer->vk) rejectDescriptor(descriptorIndex, "null-buffer-handle");
        if (bufferFormattedView && bufferStructuredView) rejectDescriptor(descriptorIndex, "conflicting-buffer-views");
#endif
        if (bufferFormattedView != nullptr) {
#if defined(__ANDROID__)
            if (!device->descriptorResources.live(bufferFormattedView, VK_OBJECT_TYPE_BUFFER_VIEW)) rejectDescriptor(descriptorIndex, "buffer-view-not-live-or-wrong-device");
#endif
            assert((bufferStructuredView == nullptr) && "Can't use structured views and formatted views at the same time.");

            const VulkanBufferFormattedView *interfaceBufferFormattedView = static_cast<const VulkanBufferFormattedView *>(bufferFormattedView);
#if defined(__ANDROID__)
            writeRecord.view = uint64_t(interfaceBufferFormattedView->vk);
            writeRecord.format = toVk(interfaceBufferFormattedView->format);
            if (interfaceBufferFormattedView->buffer != interfaceBuffer) rejectDescriptor(descriptorIndex, "buffer-view-parent-mismatch");
            if (device->descriptorResources.objects.at(bufferFormattedView).handle != uint64_t(interfaceBufferFormattedView->vk)) rejectDescriptor(descriptorIndex, "buffer-view-handle-mismatch");
            if (!interfaceBufferFormattedView->vk) rejectDescriptor(descriptorIndex, "null-buffer-view");
#endif
            bufferView = &interfaceBufferFormattedView->vk;
        }
        else if (bufferStructuredView != nullptr) {
            assert((bufferFormattedView == nullptr) && "Can't use structured views and formatted views at the same time.");
            assert(bufferStructuredView->structureByteStride > 0);

#if defined(__ANDROID__)
            if (!bufferStructuredView->structureByteStride) rejectDescriptor(descriptorIndex, "zero-structured-stride");
#endif
            bufferInfo.offset = uint64_t(bufferStructuredView->firstElement) * bufferStructuredView->structureByteStride;
        }
        else {
            bufferInfo.offset = 0;
        }

#if defined(__ANDROID__)
        if (!bufferView) {
            if (bufferInfo.offset >= interfaceBuffer->desc.size) rejectDescriptor(descriptorIndex, "buffer-offset-out-of-bounds");
            if (!bufferSize) bufferInfo.range = interfaceBuffer->desc.size - bufferInfo.offset;
            writeRecord.offset = bufferInfo.offset;
            writeRecord.range = bufferInfo.range == VK_WHOLE_SIZE ? interfaceBuffer->desc.size - bufferInfo.offset : bufferInfo.range;
            if (!bufferInfo.range || (bufferInfo.range != VK_WHOLE_SIZE && bufferInfo.range > interfaceBuffer->desc.size - bufferInfo.offset))
                rejectDescriptor(descriptorIndex, "buffer-range-out-of-bounds");
        }
        setDescriptor(descriptorIndex, bufferView ? nullptr : &bufferInfo, nullptr, bufferView, nullptr);
#else
        setDescriptor(descriptorIndex, &bufferInfo, nullptr, bufferView, nullptr);
#endif
    }

    void VulkanDescriptorSet::setTexture(uint32_t descriptorIndex, const RenderTexture *texture, const RenderTextureLayout textureLayout, const RenderTextureView *textureView) {
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        writeRecord = {}; writeRecord.operation = "texture";
        validateDescriptorIndex(descriptorIndex);
        if (!texture) rejectDescriptor(descriptorIndex, "null-texture-object");
        if (!device->descriptorResources.live(texture, VK_OBJECT_TYPE_IMAGE)) rejectDescriptor(descriptorIndex, "image-not-live-or-wrong-device");
#else
        if (texture == nullptr) return;
#endif
        const VulkanTexture *interfaceTexture = static_cast<const VulkanTexture *>(texture);
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = toImageLayout(textureLayout);
#if defined(__ANDROID__)
        writeRecord.resource = uint64_t(interfaceTexture->vk);
        writeRecord.format = interfaceTexture->imageFormat;
        writeRecord.usage = interfaceTexture->imageUsage;
        writeRecord.samples = interfaceTexture->desc.multisampling.sampleCount;
        writeRecord.layout = imageInfo.imageLayout;
        if (device->descriptorResources.objects.at(texture).handle != uint64_t(interfaceTexture->vk)) rejectDescriptor(descriptorIndex, "image-handle-mismatch");
        if (!interfaceTexture->vk) rejectDescriptor(descriptorIndex, "null-image-handle");
#endif

        if (textureView != nullptr) {
#if defined(__ANDROID__)
            if (!device->descriptorResources.live(textureView, VK_OBJECT_TYPE_IMAGE_VIEW)) rejectDescriptor(descriptorIndex, "image-view-not-live-or-wrong-device");
#endif
            const VulkanTextureView *interfaceTextureView = static_cast<const VulkanTextureView *>(textureView);
#if defined(__ANDROID__)
            if (device->descriptorResources.objects.at(textureView).handle != uint64_t(interfaceTextureView->vk)) rejectDescriptor(descriptorIndex, "image-view-handle-mismatch");
            if (interfaceTextureView->texture != interfaceTexture) rejectDescriptor(descriptorIndex, "image-view-parent-mismatch");
#endif
            imageInfo.imageView = interfaceTextureView->vk;
        }
        else {
            imageInfo.imageView = (interfaceTexture != nullptr) ? interfaceTexture->imageView : VK_NULL_HANDLE;
        }

#if defined(__ANDROID__)
        writeRecord.view = uint64_t(imageInfo.imageView);
        if (!imageInfo.imageView) rejectDescriptor(descriptorIndex, "null-image-view");
#endif
        setDescriptor(descriptorIndex, nullptr, &imageInfo, nullptr, nullptr);
    }

    void VulkanDescriptorSet::setSampler(uint32_t descriptorIndex, const RenderSampler *sampler) {
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        writeRecord = {}; writeRecord.operation = "sampler";
        validateDescriptorIndex(descriptorIndex);
        if (!sampler) rejectDescriptor(descriptorIndex, "null-sampler-object");
        if (!device->descriptorResources.live(sampler, VK_OBJECT_TYPE_SAMPLER)) rejectDescriptor(descriptorIndex, "sampler-not-live-or-wrong-device");
#else
        if (sampler == nullptr) return;
#endif
        const VulkanSampler *interfaceSampler = static_cast<const VulkanSampler *>(sampler);
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.sampler = interfaceSampler->vk;
#if defined(__ANDROID__)
        writeRecord.resource = uint64_t(interfaceSampler->vk);
        if (device->descriptorResources.objects.at(sampler).handle != uint64_t(interfaceSampler->vk)) rejectDescriptor(descriptorIndex, "sampler-handle-mismatch");
        if (!interfaceSampler->vk) rejectDescriptor(descriptorIndex, "null-sampler-handle");
#endif
        setDescriptor(descriptorIndex, nullptr, &imageInfo, nullptr, nullptr);
    }

    void VulkanDescriptorSet::setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) {
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        writeRecord = {}; writeRecord.operation = "acceleration";
        validateDescriptorIndex(descriptorIndex);
        if (!accelerationStructure) rejectDescriptor(descriptorIndex, "null-acceleration-object");
        if (!device->descriptorResources.live(accelerationStructure, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR))
            rejectDescriptor(descriptorIndex, "acceleration-not-live-or-wrong-device");
#else
        if (accelerationStructure == nullptr) return;
#endif
        const VulkanAccelerationStructure *interfaceAccelerationStructure = static_cast<const VulkanAccelerationStructure *>(accelerationStructure);
#if defined(__ANDROID__)
        writeRecord.resource = uint64_t(interfaceAccelerationStructure->vk);
#endif
        VkWriteDescriptorSetAccelerationStructureKHR setAccelerationStructure = {};
        setAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        setAccelerationStructure.pAccelerationStructures = &interfaceAccelerationStructure->vk;
        setAccelerationStructure.accelerationStructureCount = 1;
        setDescriptor(descriptorIndex, nullptr, nullptr, nullptr, &setAccelerationStructure);
    }

    void VulkanDescriptorSet::setDescriptor(uint32_t descriptorIndex, const VkDescriptorBufferInfo *bufferInfo, const VkDescriptorImageInfo *imageInfo, const VkBufferView *texelBufferView, void *pNext) {
#if defined(__ANDROID__)
        const std::lock_guard lock(device->descriptorResources.mutex);
        validateDescriptorIndex(descriptorIndex);
#endif
        assert(descriptorIndex < setLayout->descriptorBindingIndices.size());

        const uint32_t indexBase = setLayout->descriptorIndexBases[descriptorIndex];
        const uint32_t bindingIndex = setLayout->descriptorBindingIndices[descriptorIndex];
        const VkDescriptorSetLayoutBinding &setLayoutBinding = setLayout->setBindings[bindingIndex];
        VkWriteDescriptorSet writeDescriptor = {};
        writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptor.pNext = pNext;
        writeDescriptor.dstSet = vk;
        writeDescriptor.dstBinding = setLayoutBinding.binding;
        writeDescriptor.dstArrayElement = descriptorIndex - indexBase;
        writeDescriptor.descriptorCount = 1;
        writeDescriptor.descriptorType = setLayoutBinding.descriptorType;
        writeDescriptor.pBufferInfo = bufferInfo;
        writeDescriptor.pImageInfo = imageInfo;
        writeDescriptor.pTexelBufferView = texelBufferView;

#if defined(__ANDROID__)
        const auto type = writeDescriptor.descriptorType;
        if (bufferInfo) {
            if (imageInfo || texelBufferView || pNext || (type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && type != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER))
                rejectDescriptor(descriptorIndex, "buffer-payload-type-mismatch");
            const bool uniform = type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            const uint32_t required = uniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            if (!(writeRecord.usage & required)) rejectDescriptor(descriptorIndex, "buffer-usage-mismatch");
            const auto &limits = device->physicalDeviceProperties.limits;
            const uint64_t alignment = uniform ? limits.minUniformBufferOffsetAlignment : limits.minStorageBufferOffsetAlignment;
            const uint64_t maximum = uniform ? limits.maxUniformBufferRange : limits.maxStorageBufferRange;
            if (alignment && bufferInfo->offset % alignment) rejectDescriptor(descriptorIndex, "buffer-offset-misaligned");
            if (maximum && writeRecord.range > maximum) rejectDescriptor(descriptorIndex, "buffer-range-exceeds-device-limit");
        }
        else if (texelBufferView) {
            if (imageInfo || pNext || (type != VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER && type != VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER))
                rejectDescriptor(descriptorIndex, "texel-payload-type-mismatch");
            const uint32_t required = type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
            if ((writeRecord.usage & required) != required) rejectDescriptor(descriptorIndex, "texel-buffer-usage-mismatch");
            if (!*texelBufferView) rejectDescriptor(descriptorIndex, "null-buffer-view");
        }
        else if (imageInfo) {
            if (pNext) rejectDescriptor(descriptorIndex, "image-payload-type-mismatch");
            if (type == VK_DESCRIPTOR_TYPE_SAMPLER) {
                if (imageInfo->imageView || !imageInfo->sampler) rejectDescriptor(descriptorIndex, "sampler-payload-type-mismatch");
                if (setLayoutBinding.pImmutableSamplers) rejectDescriptor(descriptorIndex, "immutable-sampler-update");
            }
            else if (type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
                if (imageInfo->sampler || !imageInfo->imageView) rejectDescriptor(descriptorIndex, "image-payload-type-mismatch");
                const VkImageUsageFlags required = type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT;
                if (!(writeRecord.usage & required)) rejectDescriptor(descriptorIndex, "image-usage-mismatch");
                if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && imageInfo->imageLayout != VK_IMAGE_LAYOUT_GENERAL)
                    rejectDescriptor(descriptorIndex, "storage-image-layout-not-general");
                if (type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && imageInfo->imageLayout != VK_IMAGE_LAYOUT_GENERAL &&
                    imageInfo->imageLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                    imageInfo->imageLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
                    rejectDescriptor(descriptorIndex, "sampled-image-layout-invalid");
            }
            else rejectDescriptor(descriptorIndex, "image-payload-type-mismatch");
        }
        else if (pNext) {
            const auto *as = static_cast<const VkWriteDescriptorSetAccelerationStructureKHR *>(pNext);
            if (type != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR || as->sType != VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR ||
                as->accelerationStructureCount != 1 || !as->pAccelerationStructures || !*as->pAccelerationStructures)
                rejectDescriptor(descriptorIndex, "acceleration-payload-type-mismatch");
        }
        else rejectDescriptor(descriptorIndex, "missing-required-payload");

        // Logging survives launcher warm-up: first writes per set and the first
        // (descriptor type, format) on each thread are recorded independently.
        thread_local std::unordered_set<uint64_t> loggedTypes;
        const uint64_t signature = uint64_t(type) << 32 | writeRecord.format;
        const bool logWrite = diagnosticWrites < 16 || loggedTypes.find(signature) == loggedTypes.end();
        char record[512];
        if (logWrite) {
            std::snprintf(record, sizeof(record),
                "Descriptor write begin: pid=%d tid=%d op=%s ctx=%s set=%llx index=%u binding=%u element=%u type=%u allocated=%u resource=%llx view=%llx offset=%llu range=%llu format=%u usage=0x%X flags=0x%X samples=%u layout=%u",
                int(getpid()), int(gettid()), writeRecord.operation, tooie::android::descriptorContext(), (unsigned long long)vk, descriptorIndex, writeDescriptor.dstBinding, writeDescriptor.dstArrayElement,
                unsigned(type), allocatedCounts[bindingIndex], (unsigned long long)writeRecord.resource, (unsigned long long)writeRecord.view,
                (unsigned long long)writeRecord.offset, (unsigned long long)writeRecord.range, writeRecord.format, writeRecord.usage, writeRecord.flags, writeRecord.samples, writeRecord.layout);
            tooie_android_debug_log(record);
            std::fprintf(stderr, "%s\n", record);
        }
#endif

        vkUpdateDescriptorSets(device->vk, 1, &writeDescriptor, 0, nullptr);
#if defined(__ANDROID__)
        ++diagnosticWrites;
        if (logWrite) {
            loggedTypes.insert(signature);
            std::snprintf(record, sizeof(record), "Descriptor write returned: pid=%d tid=%d set=%llx index=%u binding=%u type=%u",
                int(getpid()), int(gettid()), (unsigned long long)vk, descriptorIndex, writeDescriptor.dstBinding, unsigned(type));
            tooie_android_debug_log(record);
        }
#endif
    }

    VkDescriptorPool VulkanDescriptorSet::createDescriptorPool(VulkanDevice *device, const std::unordered_map<VkDescriptorType, uint32_t> &typeCounts, bool lastRangeIsBoundless, VkResult *creationResult) {
        thread_local std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.clear();

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        for (auto it : typeCounts) {
            VkDescriptorPoolSize poolSize = {};
            poolSize.type = it.first;
            poolSize.descriptorCount = it.second;
            poolSizes.emplace_back(poolSize);
        }

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.pPoolSizes = !poolSizes.empty() ? poolSizes.data() : nullptr;
        poolInfo.poolSizeCount = uint32_t(poolSizes.size());

        if (lastRangeIsBoundless) {
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        }

#if defined(__ANDROID__)
        char record[384];
        for (const auto &size : poolSizes) {
            std::snprintf(record, sizeof(record), "Descriptor pool capacity: pid=%d tid=%d ctx=%s type=%u capacity=%u flags=0x%X maxSets=%u types=%u",
                int(getpid()), int(gettid()), tooie::android::descriptorContext(), unsigned(size.type), size.descriptorCount, poolInfo.flags, poolInfo.maxSets, poolInfo.poolSizeCount);
            tooie_android_debug_log(record);
        }
#endif

        VkResult res = vkCreateDescriptorPool(device->vk, &poolInfo, nullptr, &descriptorPool);
        if (creationResult) *creationResult = res;
#if defined(__ANDROID__)
        std::snprintf(record, sizeof(record), "Descriptor pool creation returned: pid=%d tid=%d ctx=%s result=%d pool=%llx flags=0x%X maxSets=%u device=%llx",
            int(getpid()), int(gettid()), tooie::android::descriptorContext(), int(res), (unsigned long long)descriptorPool, poolInfo.flags, poolInfo.maxSets, (unsigned long long)device->vk);
        tooie_android_debug_log(record);
#endif
        if (res == VK_SUCCESS) {
            return descriptorPool;
        }
        else {
            fprintf(stderr, "vkCreateDescriptorPool failed with error code 0x%X.\n", res);
#if defined(__ANDROID__)
            char record[256];
            std::snprintf(record, sizeof(record), "Descriptor pool creation failed: result=%d typeCount=%u variable=%u", int(res), unsigned(poolSizes.size()), unsigned(lastRangeIsBoundless));
            tooie_android_debug_log(record);
#endif
            return VK_NULL_HANDLE;
        }
    }

    // VulkanSwapChain

    VulkanSwapChain::VulkanSwapChain(VulkanCommandQueue *commandQueue, const RenderSwapChainDesc &desc) {
        tooie_android_debug_log("Plume: swapchain constructor entered");
        assert(commandQueue != nullptr);
        assert(desc.textureCount > 0);

        this->commandQueue = commandQueue;
        this->desc = desc;
        initializeSurface(desc);
    }

    void VulkanSwapChain::initializeSurface(const RenderSwapChainDesc &desc) {
        char requestedRecord[256];
        std::snprintf(requestedRecord, sizeof(requestedRecord),
            "Surface format request: vkFormat=%u rhiFormat=%u instanceApi=0x%X deviceApi=0x%X",
            unsigned(toVk(desc.format)), unsigned(desc.format), commandQueue->device->renderInterface->appInfo.apiVersion,
            commandQueue->device->physicalDeviceProperties.apiVersion);
        tooie_android_debug_log(requestedRecord);
        std::fprintf(stderr, "%s\n", requestedRecord);

        VkResult res;

#   ifdef _WIN64
        assert(desc.renderWindow != 0);
        VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.hwnd = HWND(desc.renderWindow);
        surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);

        VulkanInterface *renderInterface = commandQueue->device->renderInterface;
        res = vkCreateWin32SurfaceKHR(renderInterface->instance, &surfaceCreateInfo, nullptr, &surface);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateWin32SurfaceKHR failed with error code 0x%X.\n", res);
            failInitialization("surface-creation-failed", res);
            return;
        }
#   elif defined(__ANDROID__)
        if (!desc.renderWindow) {
            failInitialization("null-native-window");
            return;
        }
        // SDL releases its own reference in onNativeSurfaceDestroyed. WSI owns
        // a separate reference until its VkSurfaceKHR has been destroyed.
        ANativeWindow_acquire(desc.renderWindow);
        surfaceSuspended = false;
        VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.window = desc.renderWindow;

        VulkanInterface *renderInterface = commandQueue->device->renderInterface;
        tooie_android_debug_log("Plume: Android surface creation begin");
        res = vkCreateAndroidSurfaceKHR(renderInterface->instance, &surfaceCreateInfo, nullptr, &surface);
        if (res != VK_SUCCESS) {
            tooie_android_debug_log("Plume: Android surface creation failed");
            fprintf(stderr, "vkCreateAndroidSurfaceKHR failed with error code 0x%X.\n", res);
            failInitialization("surface-creation-failed", res);
            return;
        }
        tooie_android_debug_log("Plume: Android surface created");
#   elif defined(PLUME_SDL_VULKAN_ENABLED)
        VulkanInterface *renderInterface = commandQueue->device->renderInterface;
        SDL_bool sdlRes = SDL_Vulkan_CreateSurface(desc.renderWindow, renderInterface->instance, &surface);
        if (sdlRes == SDL_FALSE) {
            fprintf(stderr, "SDL_Vulkan_CreateSurface failed with error %s.\n", SDL_GetError());
            failInitialization("surface-creation-failed");
            return;
        }
#   elif defined(__linux__)
        assert(desc.renderWindow.display != 0);
        assert(desc.renderWindow.window != 0);
        VkXlibSurfaceCreateInfoKHR surfaceCreateInfo = {};
        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.dpy = desc.renderWindow.display;
        surfaceCreateInfo.window = desc.renderWindow.window;

        VulkanInterface *renderInterface = commandQueue->device->renderInterface;
        res = vkCreateXlibSurfaceKHR(renderInterface->instance, &surfaceCreateInfo, nullptr, &surface);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateXlibSurfaceKHR failed with error code 0x%X.\n", res);
            failInitialization("surface-creation-failed", res);
            return;
        }
#   elif defined(__APPLE__)
        assert(desc.renderWindow.window != 0);
        assert(desc.renderWindow.view != 0);
        // Creates a wrapper around the window for storing and fetching sizes.
        this->windowWrapper = std::make_unique<CocoaWindow>(desc.renderWindow.window);
        
        VkMetalSurfaceCreateInfoEXT surfaceCreateInfo = {};
        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        surfaceCreateInfo.pLayer = desc.renderWindow.view;

        VulkanInterface *renderInterface = commandQueue->device->renderInterface;
        res = vkCreateMetalSurfaceEXT(renderInterface->instance, &surfaceCreateInfo, nullptr, &surface);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateMetalSurfaceEXT failed with error code 0x%X.\n", res);
            return;
        }
#   endif

        VkBool32 presentSupported = false;
        VkPhysicalDevice physicalDevice = commandQueue->device->physicalDevice;
        res = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, commandQueue->familyIndex, surface, &presentSupported);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkGetPhysicalDeviceSurfaceSupportKHR failed with error code 0x%X.\n", res);
            failInitialization("present-support-query-failed", res);
            return;
        }

        if (!presentSupported) {
            fprintf(stderr, "Command queue does not support present.\n");
            failInitialization("queue-present-unsupported");
            return;
        }

        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
        if (res != VK_SUCCESS) {
            std::fprintf(stderr, "Swapchain capabilities query failed: result=%d\n", int(res));
            tooie_android_debug_log("Swapchain rejected: surface-capabilities-query-failed");
            failInitialization("surface-capabilities-query-failed", res);
            return;
        }

        // Pick an alpha compositing mode
        if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
            pickedAlphaFlag = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        }
        else if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
            pickedAlphaFlag = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        }
        else {
            fprintf(stderr, "No known supported alpha compositing mode\n");
            failInitialization("composite-alpha-unsupported");
            return;
        }

        // Make sure maxImageCount is never below minImageCount, as it's allowed to be zero.
        surfaceCapabilities.maxImageCount = std::max(surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

        // Clamp the requested buffer count between the bounds of the surface capabilities.
        this->desc.textureCount = std::clamp(desc.textureCount, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

        const VkFormat requestedFormat = toVk(desc.format);
        if (requestedFormat == VK_FORMAT_UNDEFINED || requestedFormat == VK_FORMAT_MAX_ENUM) {
            failInitialization("invalid-requested-format");
            return;
        }
        // The list can change between count/data queries. Never select from a
        // partial list or from zero-filled slots after an unchecked query error.
        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        bool completeFormats = false;
        constexpr uint32_t MaxFormatAttempts = 4;
        for (uint32_t attempt = 0; attempt < MaxFormatAttempts; ++attempt) {
            uint32_t count = 0;
            res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr);
            char record[256];
            std::snprintf(record, sizeof(record),
                "Surface format count: attempt=%u result=%d count=%u requested=%u",
                attempt + 1, int(res), count, unsigned(requestedFormat));
            tooie_android_debug_log(record);
            std::fprintf(stderr, "%s\n", record);
            if (res != VK_SUCCESS || count == 0) {
                failInitialization(res != VK_SUCCESS ? "surface-format-count-query-failed" : "surface-format-count-zero", res);
                return;
            }
            surfaceFormats.assign(count, VkSurfaceFormatKHR{});
            const uint32_t capacity = count;
            res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, surfaceFormats.data());
            std::snprintf(record, sizeof(record),
                "Surface format data: attempt=%u result=%d written=%u capacity=%u requested=%u",
                attempt + 1, int(res), count, capacity, unsigned(requestedFormat));
            tooie_android_debug_log(record);
            std::fprintf(stderr, "%s\n", record);
            if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
                failInitialization("surface-format-data-query-failed", res);
                return;
            }
            // Log every valid returned pair, including the prefix of an
            // incomplete list, before deciding whether selection is possible.
            for (uint32_t i = 0; i < std::min(count, capacity); ++i) {
                std::snprintf(record, sizeof(record),
                    "Surface format pair: attempt=%u index=%u format=%u colorSpace=%u complete=%u",
                    attempt + 1, i, unsigned(surfaceFormats[i].format), unsigned(surfaceFormats[i].colorSpace), unsigned(res == VK_SUCCESS));
                tooie_android_debug_log(record);
                std::fprintf(stderr, "%s\n", record);
            }
            if (count == 0 || count > capacity) {
                failInitialization("surface-format-data-count-invalid", res);
                return;
            }
            if (res == VK_INCOMPLETE) continue;
            surfaceFormats.resize(count); // A smaller successful result is valid.
            completeFormats = true;
            break;
        }
        if (!completeFormats) {
            failInitialization("surface-format-enumeration-incomplete", VK_INCOMPLETE);
            return;
        }

        // VK_FORMAT_UNDEFINED was a Vulkan 1.0 'any format' convention. Vulkan
        // 1.1+ forbids it in this query. This backend requests Vulkan 1.2: do not
        // treat the sentinel as a reported BGRA8 capability or pass it to WSI.
        VkSurfaceFormatKHR selectedFormat = {};
        for (const VkSurfaceFormatKHR &format : surfaceFormats) {
            if (format.format == VK_FORMAT_UNDEFINED) {
                failInitialization("undefined-surface-format-vulkan-1.2");
                return;
            }
            if (format.format == requestedFormat &&
                (selectedFormat.format == VK_FORMAT_UNDEFINED || format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)) {
                selectedFormat = format;
            }
        }
        RenderFormat selectedRhiFormat = desc.format;
#if defined(__ANDROID__)
        // Android native surfaces commonly omit BGRA8 even when owned BGRA
        // images are supported. Use only an actually enumerated RGBA8 UNORM /
        // sRGB nonlinear pair, after the preferred requested format is absent.
        if (selectedFormat.format == VK_FORMAT_UNDEFINED && requestedFormat == VK_FORMAT_B8G8R8A8_UNORM) {
            for (const VkSurfaceFormatKHR &format : surfaceFormats) {
                if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    selectedFormat = format;
                    selectedRhiFormat = RenderFormat::R8G8B8A8_UNORM;
                    break;
                }
            }
        }
#endif
        if (selectedFormat.format == VK_FORMAT_UNDEFINED) {
            failInitialization("requested-surface-format-unsupported");
            return;
        }

#if defined(__ANDROID__)
        VkFormatProperties formatProperties = {};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, selectedFormat.format, &formatProperties);
        const VkFormatFeatureFlags requiredFeatures = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        const VkFormatFeatureFlags missingFeatures = requiredFeatures & ~formatProperties.optimalTilingFeatures;
        char featuresRecord[256];
        std::snprintf(featuresRecord, sizeof(featuresRecord),
            "Surface format features: selected=%u optimal=0x%X required=0x%X missing=0x%X",
            unsigned(selectedFormat.format), formatProperties.optimalTilingFeatures, requiredFeatures, missingFeatures);
        tooie_android_debug_log(featuresRecord);
        std::fprintf(stderr, "%s\n", featuresRecord);
        if (missingFeatures != 0) {
            failInitialization("surface-format-features-unsupported");
            return;
        }
#endif
        uint32_t presentModeCount = 0;
        res = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if (res != VK_SUCCESS || presentModeCount == 0) {
            failInitialization("present-mode-count-query-failed", res);
            return;
        }
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        const uint32_t presentModeCapacity = presentModeCount;
        res = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
        if (res != VK_SUCCESS || presentModeCount == 0 || presentModeCount > presentModeCapacity) {
            failInitialization("present-mode-data-query-failed", res);
            return;
        }
        presentModes.resize(presentModeCount);
        immediatePresentModeSupported = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end();
        mailboxPresentModeSupported = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end();

        // Sets the required presentation mode.
        setVsyncEnabled(true);

        // Pick an alpha compositing mode, prefer opaque over inherit.
        if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
            pickedAlphaFlag = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        }
        else if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
            pickedAlphaFlag = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        }
        else {
            fprintf(stderr, "No supported alpha compositing mode was found.\n");
            failInitialization("composite-alpha-unsupported");
            return;
        }

        // Publish selection only after every constructor prerequisite succeeds.
        pickedSurfaceFormat = selectedFormat;
        this->desc.format = selectedRhiFormat;
        initialized = true;
        initializationFailure = "none";
        char selectedRecord[256];
        std::snprintf(selectedRecord, sizeof(selectedRecord),
            "Surface format selected: requested=%u selected=%u colorSpace=%u rhiFormat=%u initialized=1",
            unsigned(requestedFormat), unsigned(pickedSurfaceFormat.format), unsigned(pickedSurfaceFormat.colorSpace), unsigned(this->desc.format));
        tooie_android_debug_log(selectedRecord);
        std::fprintf(stderr, "%s\n", selectedRecord);
        // Parent command queue should track this swap chain.
        commandQueue->swapChains.insert(this);
    }

    void VulkanSwapChain::failInitialization(const char *reason, VkResult result) {
        initialized = false;
        initializationFailure = reason;
        pickedSurfaceFormat = {};
        char message[256];
        std::snprintf(message, sizeof(message),
            "Swapchain initialization rejected: reason=%s result=%d requestedFormat=%u rhiFormat=%u",
            reason, int(result), unsigned(toVk(desc.format)), unsigned(desc.format));
        tooie_android_debug_log(message);
        std::fprintf(stderr, "%s\n", message);
    }

    VulkanSwapChain::~VulkanSwapChain() {
        releaseImageViews();
        releaseSwapChain();

        if (surface != VK_NULL_HANDLE) {
            VulkanInterface *renderInterface = commandQueue->device->renderInterface;
            vkDestroySurfaceKHR(renderInterface->instance, surface, nullptr);
        }

#if defined(__ANDROID__)
        if (!surfaceSuspended && desc.renderWindow) ANativeWindow_release(desc.renderWindow);
#endif
        // Remove tracking from the parent command queue.
        commandQueue->swapChains.erase(this);
    }

    bool VulkanSwapChain::present(uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount) {
        if (isEmpty()) return false;
        thread_local std::vector<VkSemaphore> waitSemaphoresVector;
        waitSemaphoresVector.clear();
        for (uint32_t i = 0; i < waitSemaphoreCount; i++) {
            VulkanCommandSemaphore *interfaceSemaphore = (VulkanCommandSemaphore *)(waitSemaphores[i]);
            waitSemaphoresVector.emplace_back(interfaceSemaphore->vk);
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pSwapchains = &vk;
        presentInfo.swapchainCount = 1;
        presentInfo.pImageIndices = &textureIndex;
        presentInfo.pWaitSemaphores = !waitSemaphoresVector.empty() ? waitSemaphoresVector.data() : nullptr;
        presentInfo.waitSemaphoreCount = uint32_t(waitSemaphoresVector.size());

        VkPresentIdKHR presentId = {};
        if (desc.enablePresentWait) {
            assert(commandQueue->device->capabilities.presentWait && "Present wait must be supported by the RenderDevice.");

            currentPresentId++;
            presentId.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
            presentId.pPresentIds = &currentPresentId;
            presentId.swapchainCount = 1;
            presentInfo.pNext = &presentId;
        }
        
        VkResult res;
        {
            const std::scoped_lock queueLock(*commandQueue->queue->mutex);
            res = vkQueuePresentKHR(commandQueue->queue->vk, &presentInfo);
        }

#if defined(__ANDROID__)
        if (res == VK_ERROR_SURFACE_LOST_KHR) surfaceLost = true;
        if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
            std::fprintf(stderr, "Android present unavailable: result=%d\n", int(res));
#endif
#if defined(__APPLE__)
        // Under MoltenVK, VK_SUBOPTIMAL_KHR does not result in a valid state for rendering. We intentionally
        // only check for this error during present to avoid having to synchronize manually against the semaphore
        // signalled by vkAcquireNextImageKHR.
        if (res != VK_SUCCESS) {
#else
        if ((res != VK_SUCCESS) && (res != VK_SUBOPTIMAL_KHR)) {
#endif
            return false;
        }

        return true;
    }

    void VulkanSwapChain::wait() {
        if (isEmpty()) return;
        assert(desc.enablePresentWait && "Present wait should've been explicitly enabled during swap chain creation before using the wait function.");

        if (currentPresentId >= desc.maxFrameLatency) {
            constexpr uint64_t waitTimeout = 100000000;
            vkWaitForPresentKHR(commandQueue->device->vk, vk, currentPresentId - (desc.maxFrameLatency - 1), waitTimeout);
        }
    }

    bool VulkanSwapChain::resize() {
        // C++ constructor early returns leave an object behind. No WSI call or
        // resource mutation is legal until all initialization has succeeded.
        if (!initialized || surface == VK_NULL_HANDLE || pickedSurfaceFormat.format == VK_FORMAT_UNDEFINED ||
            pickedSurfaceFormat.format != toVk(desc.format)) {
            char message[320];
            std::snprintf(message, sizeof(message),
                "Swapchain resize rejected: initialized=%u initReason=%s selectedFormat=%u requestedFormat=%u surface=%llx",
                unsigned(initialized), initializationFailure, unsigned(pickedSurfaceFormat.format), unsigned(toVk(desc.format)),
                (unsigned long long)surface);
            tooie_android_debug_log(message);
            std::fprintf(stderr, "%s\n", message);
            return false;
        }
#if defined(__ANDROID__)
        if (surfaceSuspended || surfaceLost) return false;
#endif
        getWindowSize(width, height);

        // Don't recreate the swap chain at all if the window doesn't have a valid size.
        if ((width == 0) || (height == 0)) {
            return false;
        }

        // Query surface capabilities to get the valid extent bounds.
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        const VkResult capabilitiesResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(commandQueue->device->physicalDevice, surface, &surfaceCapabilities);
        if (capabilitiesResult != VK_SUCCESS) {
            std::fprintf(stderr, "Swapchain capabilities query failed: result=%d\n", int(capabilitiesResult));
            tooie_android_debug_log("Swapchain rejected: surface-capabilities-query-failed");
#if defined(__ANDROID__)
            if (capabilitiesResult == VK_ERROR_SURFACE_LOST_KHR) surfaceLost = true;
#endif
            return false;
        }

        // Clamp the extent to the surface capabilities' min/max bounds.
        // This is required because the window size may differ from the valid surface extent
        // (e.g., due to window decorations, compositor behavior, or timing issues).
#if defined(__ANDROID__)
        if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
            width = surfaceCapabilities.currentExtent.width;
            height = surfaceCapabilities.currentExtent.height;
        }
        else
#endif
        {
            width = std::clamp(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
            height = std::clamp(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
        }

        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = desc.textureCount;
        createInfo.imageFormat = pickedSurfaceFormat.format;
        createInfo.imageColorSpace = pickedSurfaceFormat.colorSpace;
        createInfo.imageExtent.width = width;
        createInfo.imageExtent.height = height;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
#if defined(__ANDROID__)
        createInfo.preTransform =
            (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
                ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                : surfaceCapabilities.currentTransform;
#else
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
#endif
        createInfo.compositeAlpha = pickedAlphaFlag;
        createInfo.presentMode = requiredPresentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = vk;

#if defined(__ANDROID__)
        char swapchainRecord[256];
        std::snprintf(swapchainRecord, sizeof(swapchainRecord),
            "Swapchain record: format=%u colorSpace=%u rhiFormat=%u initialized=%u extent=%ux%u usage=0x%X supportedUsage=0x%X transform=0x%X family=%u",
            unsigned(createInfo.imageFormat), unsigned(createInfo.imageColorSpace), unsigned(desc.format), unsigned(initialized), width, height, createInfo.imageUsage,
            surfaceCapabilities.supportedUsageFlags, createInfo.preTransform, commandQueue->familyIndex);
        tooie_android_debug_log(swapchainRecord);
        const VkImageUsageFlags missingUsage = createInfo.imageUsage & ~surfaceCapabilities.supportedUsageFlags;
        if (missingUsage != 0) {
            char message[256];
            std::snprintf(message, sizeof(message),
                "Swapchain rejected: unsupported-image-usage requested=0x%X supported=0x%X missing=0x%X transferDstMissing=%u",
                createInfo.imageUsage, surfaceCapabilities.supportedUsageFlags, missingUsage,
                unsigned((missingUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0));
            tooie_android_debug_log(message);
            std::fprintf(stderr, "%s\n", message);
            return false;
        }
#endif
#if defined(__ANDROID__)
        // Validate the complete selected 2D optimal-tiling image configuration,
        // independently of surface usage and before creation/view destruction.
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = createInfo.imageFormat;
        imageInfo.extent = { width, height, 1 };
        imageInfo.mipLevels = imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = createInfo.imageUsage;
        if (!VulkanTexture::validateImageConfiguration(commandQueue->device, imageInfo)) {
            tooie_android_debug_log("Swapchain rejected: selected-image-configuration-unsupported");
            return false;
        }
#endif
        // Only release the old views after the replacement configuration passes.
        releaseImageViews();
        VkSwapchainKHR replacement = VK_NULL_HANDLE;
        VkResult res = vkCreateSwapchainKHR(commandQueue->device->vk, &createInfo, nullptr, &replacement);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateSwapchainKHR failed with error code 0x%X.\n", res);
#if defined(__ANDROID__)
            if (res == VK_ERROR_SURFACE_LOST_KHR) surfaceLost = true;
            releaseSwapChain();
#endif
            return false;
        }

        vk = replacement;

        // Store the chosen present mode to identify later whether the swapchain needs to be recreated.
        createdPresentMode = requiredPresentMode;

        // Reset present counter.
        presentCount = 1;

        if (createInfo.oldSwapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(commandQueue->device->vk, createInfo.oldSwapchain, nullptr);
        }

        uint32_t retrievedImageCount = 0;
        res = vkGetSwapchainImagesKHR(commandQueue->device->vk, vk, &retrievedImageCount, nullptr);
        if (res != VK_SUCCESS || retrievedImageCount < desc.textureCount) {
            releaseSwapChain();
            fprintf(stderr, "Image count differs from the texture count.\n");
            return false;
        }

        desc.textureCount = retrievedImageCount;

        std::vector<VkImage> images(desc.textureCount);
        const uint32_t imageCapacity = desc.textureCount;
        uint32_t writtenImageCount = imageCapacity;
        res = vkGetSwapchainImagesKHR(commandQueue->device->vk, vk, &writtenImageCount, images.data());
        // Image count is fixed for a created swapchain. Never mutate the RHI
        // descriptor from a failed/incomplete or inconsistent data query.
        if (res != VK_SUCCESS || writtenImageCount != imageCapacity) {
            releaseSwapChain();
            fprintf(stderr, "vkGetSwapchainImagesKHR failed with error code 0x%X.\n", res);
            return false;
        }

        // Assign the swap chain images to the buffer resources.
        textures.resize(desc.textureCount);

        for (uint32_t i = 0; i < desc.textureCount; i++) {
            textures[i] = VulkanTexture(commandQueue->device, images[i]);
            textures[i].isSwapChainImage = true;
            textures[i].imageFormat = pickedSurfaceFormat.format;
            textures[i].imageUsage = createInfo.imageUsage;
            textures[i].desc.dimension = RenderTextureDimension::TEXTURE_2D;
            textures[i].desc.format = desc.format;
            textures[i].desc.width = width;
            textures[i].desc.height = height;
            textures[i].desc.depth = 1;
            textures[i].desc.mipLevels = 1;
            textures[i].desc.arraySize = 1;
            textures[i].desc.flags = RenderTextureFlag::RENDER_TARGET;
            textures[i].fillSubresourceRange();
            textures[i].createImageView(pickedSurfaceFormat.format);
#if defined(__ANDROID__)
            if (textures[i].imageView == VK_NULL_HANDLE) {
                releaseImageViews();
                releaseSwapChain();
                tooie_android_debug_log("Swapchain rejected: image-view-creation-failed");
                return false;
            }
#endif
        }

        return true;
    }

    bool VulkanSwapChain::needsResize() const {
        if (!initialized) return true;
#if defined(__ANDROID__)
        if (surfaceSuspended || surfaceLost) return true;
#endif
        uint32_t windowWidth, windowHeight;
        getWindowSize(windowWidth, windowHeight);
        return (vk == VK_NULL_HANDLE) || (windowWidth != width) || (windowHeight != height) || (requiredPresentMode != createdPresentMode);
    }

    void VulkanSwapChain::setVsyncEnabled(bool vsyncEnabled) {
        // Immediate mode must be supported and the presentation mode will only be used on the next resize.
        // needsResize() will return as true as long as the created and required present mode do not match.
        if (immediatePresentModeSupported && !vsyncEnabled) {
            requiredPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
        // FIFO is guaranteed to be supported.
        else {
            requiredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
    }

    bool VulkanSwapChain::isVsyncEnabled() const {
        return (createdPresentMode == VK_PRESENT_MODE_FIFO_KHR) || (createdPresentMode == VK_PRESENT_MODE_MAILBOX_KHR);
    }

    uint32_t VulkanSwapChain::getWidth() const {
        return width;
    }

    uint32_t VulkanSwapChain::getHeight() const {
        return height;
    }

    RenderTexture *VulkanSwapChain::getTexture(uint32_t textureIndex) {
        return &textures[textureIndex];
    }

    uint32_t VulkanSwapChain::getTextureCount() const {
        return desc.textureCount;
    }

#if defined(__ANDROID__)
    RenderFormat VulkanSwapChain::getFormat() const {
        return initialized ? desc.format : RenderFormat::UNKNOWN;
    }

    void VulkanSwapChain::waitForIdle() {
        if (vk == VK_NULL_HANDLE) return;
        const std::scoped_lock queueLock(*commandQueue->queue->mutex);
        tooie::android::boundedQueueIdle([&] {
            const VkResult result = vkQueueWaitIdle(commandQueue->queue->vk);
            if (result != VK_SUCCESS) tooie::android::lifecycleFailure("queue-idle-error");
        });
    }

    void VulkanSwapChain::suspendSurface() {
        // The owner has drained the queue and destroyed its framebuffers first.
        releaseImageViews();
        textures.clear();
        releaseSwapChain();
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(commandQueue->device->renderInterface->instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (!surfaceSuspended && desc.renderWindow) ANativeWindow_release(desc.renderWindow);
        desc.renderWindow = nullptr;
        surfaceSuspended = true;
        surfaceLost = false;
        width = height = 0;
        currentPresentId = 0;
        tooie_android_debug_log("Android Vulkan lifecycle: WSI released before SDL surface teardown");
    }

    bool VulkanSwapChain::isSurfaceSuspended() const { return surfaceSuspended || surfaceLost; }

    bool VulkanSwapChain::resumeSurface(RenderWindow window) {
        if (!surfaceSuspended) return initialized && !surfaceLost;
        if (!window) return false;
        RenderSwapChainDesc resumedDesc = desc;
        resumedDesc.renderWindow = window;
        const bool vsync = requiredPresentMode != VK_PRESENT_MODE_IMMEDIATE_KHR;
        desc = resumedDesc;
        initialized = false;
        pickedSurfaceFormat = {};
        initializeSurface(resumedDesc);
        setVsyncEnabled(vsync);
        // Request the already selected concrete format: changing it here would
        // invalidate the existing VI/UI pipelines. Every capability is queried
        // again on the replacement surface; failure leaves presentation stopped.
        if (!initialized) { suspendSurface(); return false; }
        tooie_android_debug_log("Android Vulkan lifecycle: replacement surface validated");
        return true;
    }
#endif

    RenderWindow VulkanSwapChain::getWindow() const {
        return desc.renderWindow;
    }

    bool VulkanSwapChain::isEmpty() const {
#if defined(__ANDROID__)
        if (surfaceSuspended || surfaceLost) return true;
#endif
        return !initialized || (vk == VK_NULL_HANDLE) || (width == 0) || (height == 0);
    }

    uint32_t VulkanSwapChain::getRefreshRate() const {
        if (isEmpty()) return 0;
        VkRefreshCycleDurationGOOGLE refreshCycle = {};
        VkResult res = vkGetRefreshCycleDurationGOOGLE(commandQueue->device->vk, vk, &refreshCycle);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkGetRefreshCycleDurationGOOGLE failed with error code 0x%X.\n", res);
            return 0;
        }

        return std::lround(1000000000.0 / refreshCycle.refreshDuration);
    }

    void VulkanSwapChain::getWindowSize(uint32_t &dstWidth, uint32_t &dstHeight) const {
#   if defined(_WIN64)
        RECT rect;
        GetClientRect(desc.renderWindow, &rect);
        dstWidth = rect.right - rect.left;
        dstHeight = rect.bottom - rect.top;
#   elif defined(__ANDROID__)
        if (!desc.renderWindow || surfaceSuspended) { dstWidth = dstHeight = 0; return; }
        const int32_t w = ANativeWindow_getWidth(desc.renderWindow);
        const int32_t h = ANativeWindow_getHeight(desc.renderWindow);
        dstWidth = w > 0 ? uint32_t(w) : 0;
        dstHeight = h > 0 ? uint32_t(h) : 0;
#   elif defined(PLUME_SDL_VULKAN_ENABLED)
        SDL_GetWindowSizeInPixels(desc.renderWindow, (int *)(&dstWidth), (int *)(&dstHeight));
#   elif defined(__linux__)
        XWindowAttributes attributes;
        XGetWindowAttributes(desc.renderWindow.display, desc.renderWindow.window, &attributes);
        // The attributes width and height members do not include the border.
        dstWidth = attributes.width;
        dstHeight = attributes.height;
#   elif defined(__APPLE__)
        CocoaWindowAttributes attributes;
        windowWrapper->getWindowAttributes(&attributes);
        dstWidth = attributes.width;
        dstHeight = attributes.height;
#   endif
    }

    bool VulkanSwapChain::acquireTexture(RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex) {
        if (isEmpty()) return false;
        assert(signalSemaphore != nullptr);

        VulkanCommandSemaphore *interfaceSemaphore = static_cast<VulkanCommandSemaphore *>(signalSemaphore);
#if defined(__ANDROID__)
        // A surface can lose forward progress during an Activity transition.
        // Retry on another frame; never strand the pause handshake in acquire.
        constexpr uint64_t acquireTimeout = 100000000ULL;
#else
        constexpr uint64_t acquireTimeout = UINT64_MAX;
#endif
        VkResult res = vkAcquireNextImageKHR(commandQueue->device->vk, vk, acquireTimeout, interfaceSemaphore->vk, VK_NULL_HANDLE, textureIndex);
#if defined(__ANDROID__)
        if (res == VK_ERROR_SURFACE_LOST_KHR) surfaceLost = true;
        if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
            std::fprintf(stderr, "Android acquire unavailable: result=%d\n", int(res));
#endif
        if ((res != VK_SUCCESS) && (res != VK_SUBOPTIMAL_KHR)) {
            return false;
        }

        return true;
    }

    void VulkanSwapChain::releaseSwapChain() {
        if (vk != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(commandQueue->device->vk, vk, nullptr);
            vk = VK_NULL_HANDLE;
        }
    }

    void VulkanSwapChain::releaseImageViews() {
        for (VulkanTexture &texture : textures) {
#if defined(__ANDROID__)
            const std::lock_guard lock(commandQueue->device->descriptorResources.mutex);
            commandQueue->device->descriptorResources.erase(&texture);
#endif
            if (texture.imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(commandQueue->device->vk, texture.imageView, nullptr);
                texture.imageView = VK_NULL_HANDLE;
            }
        }
    }

    // VulkanFramebuffer
    
#if defined(__ANDROID__)
    static VkExtent2D attachmentViewExtent(const VulkanTexture *texture, const RenderTextureViewDesc &view) {
        const auto &mapping = view.componentMapping;
        const bool valid = texture->desc.dimension == RenderTextureDimension::TEXTURE_2D &&
            view.dimension == RenderTextureViewDimension::TEXTURE_2D && view.mipSlice < texture->desc.mipLevels &&
            view.mipSlice < 32 && view.arrayIndex < texture->desc.arraySize && view.arraySize != 0 &&
            std::min(view.mipLevels, texture->desc.mipLevels - view.mipSlice) == 1 &&
            mapping.r == RenderSwizzle::IDENTITY && mapping.g == RenderSwizzle::IDENTITY &&
            mapping.b == RenderSwizzle::IDENTITY && mapping.a == RenderSwizzle::IDENTITY;
        if (!valid) {
            tooie_android_debug_log("Framebuffer rejected: attachment requires a valid 2D single-mip identity view");
            std::fprintf(stderr, "Framebuffer rejected: attachment requires a valid 2D single-mip identity view\n");
            std::_Exit(EXIT_FAILURE);
        }
        return { std::max(1u, texture->desc.width >> view.mipSlice), std::max(1u, texture->desc.height >> view.mipSlice) };
    }
#endif

    VulkanFramebuffer::VulkanFramebuffer(VulkanDevice *device, const RenderFramebufferDesc &desc) {
        assert(device != nullptr);

        this->device = device;
        depthAttachmentReadOnly = desc.depthAttachmentReadOnly;

        VkResult res;
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorReferences;
        std::vector<VkImageView> imageViews;
        VkAttachmentReference depthReference = {};
        for (uint32_t i = 0; i < desc.colorAttachmentsCount; i++) {
            const VulkanTexture *colorAttachment;
            VkImageView colorAttachmentImageView;
            RenderFormat colorAttachmentFormat;
#if defined(__ANDROID__)
            RenderTextureViewDesc attachmentViewDesc;
#endif
            if (desc.colorAttachmentViews && desc.colorAttachmentViews[i]) {
                const VulkanTextureView* colorAttachmentView = static_cast<const VulkanTextureView *>(desc.colorAttachmentViews[i]);
                colorAttachment = colorAttachmentView->texture;
                colorAttachmentImageView = colorAttachmentView->vk;
                colorAttachmentFormat = colorAttachmentView->desc.format;
#if defined(__ANDROID__)
                attachmentViewDesc = colorAttachmentView->desc;
#endif
            } else {
                colorAttachment = static_cast<const VulkanTexture *>(desc.colorAttachments[i]);
                colorAttachmentImageView = colorAttachment->imageView;
                colorAttachmentFormat = colorAttachment->desc.format;
#if defined(__ANDROID__)
                attachmentViewDesc.format = colorAttachmentFormat;
                attachmentViewDesc.dimension = RenderTextureDimensionToView(colorAttachment->desc.dimension);
                attachmentViewDesc.mipLevels = colorAttachment->desc.mipLevels;
                attachmentViewDesc.arraySize = colorAttachment->desc.arraySize;
#endif
            }
#if defined(__ANDROID__)
            if (!colorAttachment->vk || !colorAttachmentImageView || toVk(colorAttachmentFormat) == VK_FORMAT_UNDEFINED ||
                (colorAttachment->isSwapChainImage && colorAttachment->imageFormat != toVk(colorAttachmentFormat))) {
                char rejection[256];
                std::snprintf(rejection, sizeof(rejection),
                    "Framebuffer rejected: reason=invalid-color-attachment-image-view-or-format attachment=%u image=%llx view=%llx imageFormat=%u viewFormat=%u swapchain=%u",
                    i, (unsigned long long)colorAttachment->vk, (unsigned long long)colorAttachmentImageView,
                    unsigned(colorAttachment->imageFormat), unsigned(toVk(colorAttachmentFormat)), unsigned(colorAttachment->isSwapChainImage));
                tooie_android_debug_log(rejection);
                std::fprintf(stderr, "%s\n", rejection);
                std::_Exit(EXIT_FAILURE);
            }
#endif
            assert((colorAttachment->desc.flags & RenderTextureFlag::RENDER_TARGET) && "Color attachment must be a render target.");
            colorAttachments.emplace_back(colorAttachment);
            imageViews.emplace_back(colorAttachmentImageView);
#if defined(__ANDROID__)
            colorAttachmentFormats.emplace_back(toVk(colorAttachmentFormat));
            colorAttachmentRhiFormats.emplace_back(colorAttachmentFormat);
            colorAttachmentImageViews.emplace_back(colorAttachmentImageView);
            colorAttachmentUsesBaseView.emplace_back(!(desc.colorAttachmentViews && desc.colorAttachmentViews[i]));
            colorAttachmentViewDescs.emplace_back(attachmentViewDesc);
            const VkExtent2D extent = attachmentViewExtent(colorAttachment, attachmentViewDesc);
            width = (i == 0) ? extent.width : std::min(width, extent.width);
            height = (i == 0) ? extent.height : std::min(height, extent.height);
#endif

#if !defined(__ANDROID__)
            if (i == 0) {
                width = uint32_t(colorAttachment->desc.width);
                height = colorAttachment->desc.height;
            }
#endif

            VkAttachmentReference reference = {};
            reference.attachment = uint32_t(attachments.size());
            reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorReferences.emplace_back(reference);

            VkAttachmentDescription attachment = {};
            attachment.format = toVk(colorAttachmentFormat);
            attachment.samples = VkSampleCountFlagBits(colorAttachment->desc.multisampling.sampleCount);
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments.emplace_back(attachment);
        }

        if (desc.depthAttachment != nullptr || desc.depthAttachmentView != nullptr) {
            VkImageView depthAttachmentImageView;
            RenderTextureViewDesc depthAttachmentViewDesc;
            if (desc.depthAttachmentView != nullptr) {
                const VulkanTextureView* depthAttachmentView = static_cast<const VulkanTextureView *>(desc.depthAttachmentView);
                depthAttachment = depthAttachmentView->texture;
                depthAttachmentImageView = depthAttachmentView->vk;
                depthAttachmentViewDesc = depthAttachmentView->desc;
            } else {
                depthAttachment = static_cast<const VulkanTexture *>(desc.depthAttachment);
                depthAttachmentImageView = depthAttachment->imageView;
                depthAttachmentViewDesc.format = depthAttachment->desc.format;
                depthAttachmentViewDesc.dimension = RenderTextureDimensionToView(depthAttachment->desc.dimension);
            }
            if (RenderFormatIsDepth(depthAttachmentViewDesc.format) && RenderFormatIsStencil(depthAttachmentViewDesc.format)) {
                // Base image view is configured for sampling depth. For framebuffer attachment,
                // create a separate view configured for both depth and stencil.
                depthAttachmentView = std::make_unique<VulkanTextureView>(depthAttachment, depthAttachmentViewDesc);
                depthAttachmentImageView = depthAttachmentView->vk;
            }
            assert((depthAttachment->desc.flags & RenderTextureFlag::DEPTH_TARGET) && "Depth attachment must be a depth target.");
            imageViews.emplace_back(depthAttachmentImageView);

#if defined(__ANDROID__)
            const VkExtent2D extent = attachmentViewExtent(depthAttachment, depthAttachmentViewDesc);
            width = (desc.colorAttachmentsCount == 0) ? extent.width : std::min(width, extent.width);
            height = (desc.colorAttachmentsCount == 0) ? extent.height : std::min(height, extent.height);
#else
            if (desc.colorAttachmentsCount == 0) {
                width = uint32_t(depthAttachment->desc.width);
                height = depthAttachment->desc.height;
            }
#endif

            depthReference.attachment = uint32_t(attachments.size());
            depthReference.layout = desc.depthAttachmentReadOnly ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            // Upgrade the operations to NONE if supported. Fixes the following validation issue: https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/2349
            // We prefer to just ignore this potential hazard on older Vulkan versions as it just seems to be an edge case for some hardware.
            const bool preferNoneForReadOnly = desc.depthAttachmentReadOnly && device->loadStoreOpNoneSupported;
            VkAttachmentDescription attachment = {};
            attachment.format = toVk(depthAttachmentViewDesc.format);
            attachment.samples = VkSampleCountFlagBits(depthAttachment->desc.multisampling.sampleCount);
            attachment.loadOp = preferNoneForReadOnly ? VK_ATTACHMENT_LOAD_OP_NONE_EXT : VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = preferNoneForReadOnly ? VK_ATTACHMENT_STORE_OP_NONE_EXT : VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = attachment.loadOp;
            attachment.stencilStoreOp = attachment.storeOp;
            attachment.initialLayout = depthReference.layout;
            attachment.finalLayout = depthReference.layout;
            attachments.emplace_back(attachment);
        }

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pColorAttachments = !colorReferences.empty() ? colorReferences.data() : nullptr;
        subpass.colorAttachmentCount = uint32_t(colorReferences.size());

        if (desc.depthAttachment != nullptr || desc.depthAttachmentView != nullptr) {
            subpass.pDepthStencilAttachment = &depthReference;
        }

        VkRenderPassCreateInfo passInfo = {};
        passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        passInfo.pAttachments = attachments.data();
        passInfo.attachmentCount = uint32_t(attachments.size());
        passInfo.pSubpasses = &subpass;
        passInfo.subpassCount = 1;

        res = vkCreateRenderPass(device->vk, &passInfo, nullptr, &renderPass);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateRenderPass failed with error code 0x%X.\n", res);
            return;
        }
        
#if defined(__ANDROID__) && defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) && !TOOIE_VULKAN_TRANSFER_CLEAR_TEST
        // Retained failing load-op control. Compatibility excludes load/store ops.
        // Load operations are compatible with the normal LOAD render pass and
        // framebuffer/pipelines. Use one variant per color attachment so a full
        // clear preserves every other attachment, including depth/stencil.
        colorClearRenderPasses.resize(desc.colorAttachmentsCount, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < desc.colorAttachmentsCount; i++) {
            attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            res = vkCreateRenderPass(device->vk, &passInfo, nullptr, &colorClearRenderPasses[i]);
            attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            if (res != VK_SUCCESS) {
                fprintf(stderr, "vkCreateRenderPass (color clear) failed with error code 0x%X.\n", res);
                std::_Exit(EXIT_FAILURE);
            }
        }
#endif

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.pAttachments = imageViews.data();
        fbInfo.attachmentCount = uint32_t(imageViews.size());
        fbInfo.width = width;
        fbInfo.height = height;
        fbInfo.layers = 1;

        res = vkCreateFramebuffer(device->vk, &fbInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateFramebuffer failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanFramebuffer::~VulkanFramebuffer() {
#if defined(__ANDROID__)
        for (VkPipeline pipeline : colorClearDrawPipelines) {
            if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device->vk, pipeline, nullptr);
        }
        if (colorClearDrawLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device->vk, colorClearDrawLayout, nullptr);
        for (VkRenderPass clearPass : colorClearRenderPasses) {
            if (clearPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(device->vk, clearPass, nullptr);
            }
        }
#endif
        if (vk != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device->vk, vk, nullptr);
        }

        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device->vk, renderPass, nullptr);
        }
    }

    uint32_t VulkanFramebuffer::getWidth() const {
        return width;
    }

    uint32_t VulkanFramebuffer::getHeight() const {
        return height;
    }

#if defined(__ANDROID__)
    RenderFormat VulkanFramebuffer::getColorAttachmentFormat(uint32_t attachmentIndex) const {
        return attachmentIndex < colorAttachmentRhiFormats.size() ? colorAttachmentRhiFormats[attachmentIndex] : RenderFormat::UNKNOWN;
    }
#endif

    bool VulkanFramebuffer::contains(const VulkanTexture *attachment) const {
        assert(attachment != nullptr);

        for (uint32_t i = 0; i < colorAttachments.size(); i++) {
            if (colorAttachments[i] == attachment) {
                return true;
            }
        }

        return (depthAttachment == attachment);
    }

    // VulkanQueryPool

    VulkanQueryPool::VulkanQueryPool(VulkanDevice *device, uint32_t queryCount) {
        assert(device != nullptr);
        assert(queryCount > 0);

        this->device = device;

        VkQueryPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        createInfo.queryCount = queryCount;
        
        VkResult res = vkCreateQueryPool(device->vk, &createInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateQueryPool failed with error code 0x%X.\n", res);
            return;
        }
        
        results.resize(queryCount);
    }

    VulkanQueryPool::~VulkanQueryPool() {
        vkDestroyQueryPool(device->vk, vk, nullptr);
    }

    void VulkanQueryPool::queryResults() {
	    VkResult res = vkGetQueryPoolResults(device->vk, vk, 0, uint32_t(results.size()), sizeof(uint64_t) * results.size(), results.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkGetQueryPoolResults failed with error code 0x%X.\n", res);
            return;
        }

        // Conversion sourced from Godot Engine's Vulkan Rendering Driver.
        auto mult64to128 = [](uint64_t u, uint64_t v, uint64_t &h, uint64_t &l) {
            uint64_t u1 = (u & 0xffffffff);
            uint64_t v1 = (v & 0xffffffff);
            uint64_t t = (u1 * v1);
            uint64_t w3 = (t & 0xffffffff);
            uint64_t k = (t >> 32);
    
            u >>= 32;
            t = (u * v1) + k;
            k = (t & 0xffffffff);
            uint64_t w1 = (t >> 32);
    
            v >>= 32;
            t = (u1 * v) + k;
            k = (t >> 32);
    
            h = (u * v) + w1 + k;
            l = (t << 32) + w3;
        };

        // Convert results to timestamps.
        constexpr uint64_t shift_bits = 16;
        double timestampPeriod = double(device->physicalDeviceProperties.limits.timestampPeriod);
        uint64_t h = 0, l = 0;
        for (uint64_t &result : results) {
            mult64to128(result, uint64_t(timestampPeriod * double(1 << shift_bits)), h, l);
            result = l;
            result >>= shift_bits;
            result |= h << (64 - shift_bits);
        }
    }

    const uint64_t *VulkanQueryPool::getResults() const {
        return results.data();
    }

    uint32_t VulkanQueryPool::getCount() const {
        return uint32_t(results.size());
    }

    // VulkanCommandList

    VulkanCommandList::VulkanCommandList(VulkanCommandQueue *queue) {
        assert(queue != nullptr);

        this->queue = queue;

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queue->device->queueFamilyIndices[toFamilyIndex(queue->type)];

        VkResult res = vkCreateCommandPool(queue->device->vk, &poolInfo, nullptr, &commandPool);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateCommandPool failed with error code 0x%X.\n", res);
            return;
        }

        VkCommandBufferAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        res = vkAllocateCommandBuffers(queue->device->vk, &allocateInfo, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkAllocateCommandBuffers failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanCommandList::~VulkanCommandList() {
        if (vk != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(queue->device->vk, commandPool, 1, &vk);
        }

        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(queue->device->vk, commandPool, nullptr);
        }
    }

    void VulkanCommandList::begin() {
#if defined(__ANDROID__)
        // Command-buffer reset invalidates every previous graphics binding.
        graphicsPipeline = VK_NULL_HANDLE;
        graphicsViewports.clear();
        graphicsScissors.clear();
        graphicsPushConstantWrites.clear();
        graphicsDepthBiasSet = false;
#endif
        vkResetCommandBuffer(vk, 0);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkResult res = vkBeginCommandBuffer(vk, &beginInfo);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkBeginCommandBuffer failed with error code 0x%X.\n", res);
            return;
        }
    }

    void VulkanCommandList::end() {
        endActiveRenderPass();

        VkResult res = vkEndCommandBuffer(vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkEndCommandBuffer failed with error code 0x%X.\n", res);
            return;
        }

        targetFramebuffer = nullptr;
        activeComputePipelineLayout = nullptr;
        activeGraphicsPipelineLayout = nullptr;
        activeRaytracingPipelineLayout = nullptr;
    }

    void VulkanCommandList::barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) {
        assert((bufferBarriersCount == 0) || (bufferBarriers != nullptr));
        assert((textureBarriersCount == 0) || (textureBarriers != nullptr));

        if ((bufferBarriersCount == 0) && (textureBarriersCount == 0)) {
            return;
        }

        endActiveRenderPass();

        const bool geometryEnabled = queue->device->capabilities.geometryShader;
        const bool rtEnabled = queue->device->capabilities.raytracing;
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | toStageFlags(stages, geometryEnabled, rtEnabled);
        thread_local std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
        thread_local std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
        bufferMemoryBarriers.clear();
        imageMemoryBarriers.clear();

        for (uint32_t i = 0; i < bufferBarriersCount; i++) {
            const RenderBufferBarrier &bufferBarrier = bufferBarriers[i];
            VulkanBuffer *interfaceBuffer = static_cast<VulkanBuffer *>(bufferBarrier.buffer);
            VkBufferMemoryBarrier bufferMemoryBarrier = {};
            bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferMemoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT; // TODO
            bufferMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT; // TODO
            bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferMemoryBarrier.buffer = interfaceBuffer->vk;
            bufferMemoryBarrier.offset = 0;
            bufferMemoryBarrier.size = interfaceBuffer->desc.size;
            bufferMemoryBarriers.emplace_back(bufferMemoryBarrier);
            srcStageMask |= toStageFlags(interfaceBuffer->barrierStages, geometryEnabled, rtEnabled);
            interfaceBuffer->barrierStages = stages;
        }

        for (uint32_t i = 0; i < textureBarriersCount; i++) {
            const RenderTextureBarrier &textureBarrier = textureBarriers[i];
            VulkanTexture *interfaceTexture = static_cast<VulkanTexture *>(textureBarrier.texture);
            VkImageMemoryBarrier imageMemoryBarrier = {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.image = interfaceTexture->vk;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT; // TODO
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT; // TODO
#if defined(__ANDROID__)
            // No previous memory accesses exist for an undefined or presented image.
            if (interfaceTexture->textureLayout == RenderTextureLayout::UNKNOWN ||
                interfaceTexture->textureLayout == RenderTextureLayout::PRESENT) {
                imageMemoryBarrier.srcAccessMask = 0;
            }

            // BOTTOM_OF_PIPE does not support memory accesses.
            if (stages == RenderBarrierStage::NONE) {
                imageMemoryBarrier.dstAccessMask = 0;
            }
#endif
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.oldLayout = toImageLayout(interfaceTexture->textureLayout);
            imageMemoryBarrier.newLayout = toImageLayout(textureBarrier.layout);
            imageMemoryBarrier.subresourceRange.levelCount = interfaceTexture->desc.mipLevels;
            imageMemoryBarrier.subresourceRange.layerCount = interfaceTexture->desc.arraySize;
            imageMemoryBarrier.subresourceRange.aspectMask = toAspectFlags(interfaceTexture->desc.format, interfaceTexture->desc.flags);
            imageMemoryBarriers.emplace_back(imageMemoryBarrier);
            srcStageMask |= toStageFlags(interfaceTexture->barrierStages, geometryEnabled, rtEnabled);
            interfaceTexture->textureLayout = textureBarrier.layout;
            interfaceTexture->barrierStages = stages;
        }
        
        if (bufferMemoryBarriers.empty() && imageMemoryBarriers.empty()) {
            return;
        }

        vkCmdPipelineBarrier(vk, srcStageMask, dstStageMask, 0, 0, nullptr, uint32_t(bufferMemoryBarriers.size()), bufferMemoryBarriers.data(), uint32_t(imageMemoryBarriers.size()), imageMemoryBarriers.data());
    }

    void VulkanCommandList::dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) {
        assert(activeComputePipelineLayout != nullptr);

        vkCmdDispatch(vk, threadGroupCountX, threadGroupCountY, threadGroupCountZ);
    }

    void VulkanCommandList::traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo &shaderBindingGroupsInfo) {
        const VulkanBuffer *interfaceBuffer = static_cast<const VulkanBuffer *>(shaderBindingTable.ref);
        assert(interfaceBuffer != nullptr);
        assert((interfaceBuffer->desc.flags & RenderBufferFlag::SHADER_BINDING_TABLE) && "Buffer must allow being used as a shader binding table.");
        assert(activeRaytracingPipelineLayout != nullptr);

        VkBufferDeviceAddressInfo tableAddressInfo = {};
        tableAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        tableAddressInfo.buffer = interfaceBuffer->vk;

        const VkDeviceAddress tableAddress = vkGetBufferDeviceAddress(queue->device->vk, &tableAddressInfo) + shaderBindingTable.offset;
        const RenderShaderBindingGroupInfo &rayGen = shaderBindingGroupsInfo.rayGen;
        const RenderShaderBindingGroupInfo &miss = shaderBindingGroupsInfo.miss;
        const RenderShaderBindingGroupInfo &hitGroup = shaderBindingGroupsInfo.hitGroup;
        const RenderShaderBindingGroupInfo &callable = shaderBindingGroupsInfo.callable;
        VkStridedDeviceAddressRegionKHR rayGenSbt = {};
        VkStridedDeviceAddressRegionKHR missSbt = {};
        VkStridedDeviceAddressRegionKHR hitSbt = {};
        VkStridedDeviceAddressRegionKHR callableSbt = {};
        rayGenSbt.deviceAddress = (rayGen.size > 0) ? (tableAddress + rayGen.offset + rayGen.startIndex * rayGen.stride) : 0;
        rayGenSbt.size = rayGen.stride; // RayGen is a special case where the size must be the same as the stride.
        rayGenSbt.stride = rayGen.stride;
        missSbt.deviceAddress = (miss.size > 0) ? (tableAddress + miss.offset + miss.startIndex * miss.stride) : 0;
        missSbt.size = miss.size;
        missSbt.stride = miss.stride;
        hitSbt.deviceAddress = (hitGroup.size > 0) ? (tableAddress + hitGroup.offset + hitGroup.startIndex * hitGroup.stride) : 0;
        hitSbt.size = hitGroup.size;
        hitSbt.stride = hitGroup.stride;
        callableSbt.deviceAddress = (callable.size > 0) ? (tableAddress + callable.offset + callable.startIndex * callable.stride) : 0;
        callableSbt.size = callable.size;
        callableSbt.stride = callable.stride;
        vkCmdTraceRaysKHR(vk, &rayGenSbt, &missSbt, &hitSbt, &callableSbt, width, height, depth);
    }

    void VulkanCommandList::drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
        assert(activeGraphicsPipelineLayout != nullptr);
        checkActiveRenderPass();

        vkCmdDraw(vk, vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
    }
    
    void VulkanCommandList::drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
        assert(activeGraphicsPipelineLayout != nullptr);
        checkActiveRenderPass();

        vkCmdDrawIndexed(vk, indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
    }

    void VulkanCommandList::setPipeline(const RenderPipeline *pipeline) {
        assert(pipeline != nullptr);

        const VulkanPipeline *interfacePipeline = static_cast<const VulkanPipeline *>(pipeline);
        switch (interfacePipeline->type) {
        case VulkanPipeline::Type::Compute: {
            const VulkanComputePipeline *computePipeline = static_cast<const VulkanComputePipeline *>(interfacePipeline);
            vkCmdBindPipeline(vk, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->vk);
            break;
        }
        case VulkanPipeline::Type::Graphics: {
            const VulkanGraphicsPipeline *graphicsPipeline = static_cast<const VulkanGraphicsPipeline *>(interfacePipeline);
            vkCmdBindPipeline(vk, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->vk);
#if defined(__ANDROID__)
            this->graphicsPipeline = graphicsPipeline->vk;
#endif
            break;
        }
        case VulkanPipeline::Type::Raytracing: {
            const VulkanRaytracingPipeline *raytracingPipeline = static_cast<const VulkanRaytracingPipeline *>(interfacePipeline);
            vkCmdBindPipeline(vk, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, raytracingPipeline->vk);
            break;
        }
        default:
            assert(false && "Unknown pipeline type.");
            break;
        }
    }

    void VulkanCommandList::setComputePipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);

        activeComputePipelineLayout = static_cast<const VulkanPipelineLayout *>(pipelineLayout);
    }

    void VulkanCommandList::setComputePushConstants(uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) {
        assert(activeComputePipelineLayout != nullptr);
        assert(rangeIndex < activeComputePipelineLayout->pushConstantRanges.size());
        
        const VkPushConstantRange &range = activeComputePipelineLayout->pushConstantRanges[rangeIndex];
        vkCmdPushConstants(vk, activeComputePipelineLayout->vk, range.stageFlags & VK_SHADER_STAGE_COMPUTE_BIT, range.offset + offset, size == 0 ? range.size : size, data);
    }

    void VulkanCommandList::setComputeDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        setDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, activeComputePipelineLayout, descriptorSet, setIndex);
    }

    void VulkanCommandList::setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);

        activeGraphicsPipelineLayout = static_cast<const VulkanPipelineLayout *>(pipelineLayout);
    }

    void VulkanCommandList::setGraphicsPushConstants(uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) {
        assert(activeGraphicsPipelineLayout != nullptr);
        assert(rangeIndex < activeGraphicsPipelineLayout->pushConstantRanges.size());

        const VkPushConstantRange &range = activeGraphicsPipelineLayout->pushConstantRanges[rangeIndex];
        vkCmdPushConstants(vk, activeGraphicsPipelineLayout->vk, range.stageFlags & VK_SHADER_STAGE_ALL_GRAPHICS, range.offset + offset, size == 0 ? range.size : size, data);
#if defined(__ANDROID__)
        // Keep the latest write to each exact range in recording order. Replaying
        // also restores partially overlapping writes and layout compatibility;
        // the helper changes only fragment push constants, no descriptor sets.
        GraphicsPushConstantWrite write = { activeGraphicsPipelineLayout->vk,
            range.stageFlags & VK_SHADER_STAGE_ALL_GRAPHICS, range.offset + offset, {} };
        const uint32_t writeSize = size == 0 ? range.size : size;
        const uint8_t *bytes = static_cast<const uint8_t *>(data);
        write.data.assign(bytes, bytes + writeSize);
        for (auto it = graphicsPushConstantWrites.begin(); it != graphicsPushConstantWrites.end();) {
            if (it->layout == write.layout && it->stages == write.stages &&
                it->offset == write.offset && it->data.size() == write.data.size()) it = graphicsPushConstantWrites.erase(it);
            else ++it;
        }
        graphicsPushConstantWrites.emplace_back(std::move(write));
#endif
    }

    void VulkanCommandList::setGraphicsDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        setDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, activeGraphicsPipelineLayout, descriptorSet, setIndex);
    }

    void VulkanCommandList::setGraphicsRootDescriptor(RenderBufferReference bufferReference, uint32_t rootDescriptorIndex) {
        assert(false && "Root descriptors are not supported in Vulkan.");
    }

    void VulkanCommandList::setRaytracingPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);

        activeRaytracingPipelineLayout = static_cast<const VulkanPipelineLayout *>(pipelineLayout);
    }

    void VulkanCommandList::setRaytracingPushConstants(uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) {
        assert(activeRaytracingPipelineLayout != nullptr);
        assert(rangeIndex < activeRaytracingPipelineLayout->pushConstantRanges.size());

        const VkPushConstantRange &range = activeRaytracingPipelineLayout->pushConstantRanges[rangeIndex];
        const VkShaderStageFlags raytracingStageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
            VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

        vkCmdPushConstants(vk, activeRaytracingPipelineLayout->vk, range.stageFlags & raytracingStageFlags, range.offset + offset, size == 0 ? range.size : size, data);
    }

    void VulkanCommandList::setRaytracingDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        setDescriptorSet(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, activeRaytracingPipelineLayout, descriptorSet, setIndex);
    }

    void VulkanCommandList::setIndexBuffer(const RenderIndexBufferView *view) {
        if (view != nullptr) {
            const VulkanBuffer *interfaceBuffer = static_cast<const VulkanBuffer *>(view->buffer.ref);
            vkCmdBindIndexBuffer(vk, (interfaceBuffer != nullptr) ? interfaceBuffer->vk : VK_NULL_HANDLE, view->buffer.offset, toIndexType(view->format));
        }
    }

    void VulkanCommandList::setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots) {
        if ((views != nullptr) && (viewCount > 0)) {
            // Input slots aren't actually used by Vulkan as the stride is baked into the pipeline, but we validate it for the sake of consistency with D3D12.
            assert(inputSlots != nullptr);

            thread_local std::vector<VkBuffer> bufferVector;
            thread_local std::vector<VkDeviceSize> offsetVector;
            bufferVector.clear();
            offsetVector.clear();
            for (uint32_t i = 0; i < viewCount; i++) {
                const VulkanBuffer *interfaceBuffer = static_cast<const VulkanBuffer *>(views[i].buffer.ref);
                if ((interfaceBuffer == nullptr) && !queue->device->nullDescriptorSupported) {
                    interfaceBuffer = static_cast<const VulkanBuffer *>(queue->device->nullBuffer.get());
                }

                bufferVector.emplace_back((interfaceBuffer != nullptr) ? interfaceBuffer->vk : VK_NULL_HANDLE);
                offsetVector.emplace_back(views[i].buffer.offset);
            }

            vkCmdBindVertexBuffers(vk, startSlot, viewCount, bufferVector.data(), offsetVector.data());
        }
    }

    void VulkanCommandList::setViewports(const RenderViewport *viewports, uint32_t count) {
#if defined(__ANDROID__)
        graphicsViewports.clear();
        for (uint32_t i = 0; i < count; ++i) graphicsViewports.push_back(
            VkViewport{ viewports[i].x, viewports[i].y, viewports[i].width, viewports[i].height, viewports[i].minDepth, viewports[i].maxDepth });
#endif
        if (count > 1) {
            thread_local std::vector<VkViewport> viewportVector;
            viewportVector.clear();

            for (uint32_t i = 0; i < count; i++) {
                viewportVector.emplace_back(VkViewport{ viewports[i].x, viewports[i].y, viewports[i].width, viewports[i].height, viewports[i].minDepth, viewports[i].maxDepth });
            }

            if (!viewportVector.empty()) {
                vkCmdSetViewport(vk, 0, uint32_t(viewportVector.size()), viewportVector.data());
            }
        }
        else {
            // Single element fast path.
            VkViewport viewport = VkViewport{ viewports[0].x, viewports[0].y, viewports[0].width, viewports[0].height, viewports[0].minDepth, viewports[0].maxDepth };
            vkCmdSetViewport(vk, 0, 1, &viewport);
        }
    }

    void VulkanCommandList::setScissors(const RenderRect *scissorRects, uint32_t count) {
#if defined(__ANDROID__)
        graphicsScissors.clear();
        for (uint32_t i = 0; i < count; ++i) graphicsScissors.push_back(VkRect2D{
            { scissorRects[i].left, scissorRects[i].top },
            { uint32_t(scissorRects[i].right - scissorRects[i].left), uint32_t(scissorRects[i].bottom - scissorRects[i].top) } });
#endif
        if (count > 1) {
            thread_local std::vector<VkRect2D> scissorVector;
            scissorVector.clear();

            for (uint32_t i = 0; i < count; i++) {
                scissorVector.emplace_back(VkRect2D{ VkOffset2D{ scissorRects[i].left, scissorRects[i].top }, VkExtent2D{ uint32_t(scissorRects[i].right - scissorRects[i].left), uint32_t(scissorRects[i].bottom - scissorRects[i].top) } });
            }

            if (!scissorVector.empty()) {
                vkCmdSetScissor(vk, 0, uint32_t(scissorVector.size()), scissorVector.data());
            }
        }
        else {
            // Single element fast path.
            VkRect2D scissor = VkRect2D{ VkOffset2D{ scissorRects[0].left, scissorRects[0].top }, VkExtent2D{ uint32_t(scissorRects[0].right - scissorRects[0].left), uint32_t(scissorRects[0].bottom - scissorRects[0].top) } };
            vkCmdSetScissor(vk, 0, 1, &scissor);
        }
    }

    void VulkanCommandList::setFramebuffer(const RenderFramebuffer *framebuffer) {
        endActiveRenderPass();

        if (framebuffer != nullptr) {
            const VulkanFramebuffer *interfaceFramebuffer = static_cast<const VulkanFramebuffer *>(framebuffer);
            targetFramebuffer = interfaceFramebuffer;
        }
        else {
            targetFramebuffer = nullptr;
        }
    }

    void VulkanCommandList::setDepthBias(float depthBias, float depthBiasClamp, float slopeScaledDepthBias) {
#if defined(__ANDROID__)
        graphicsDepthBiasSet = true;
        graphicsDepthBias[0] = depthBias;
        graphicsDepthBias[1] = depthBiasClamp;
        graphicsDepthBias[2] = slopeScaledDepthBias;
#endif
        vkCmdSetDepthBias(vk, depthBias, depthBiasClamp, slopeScaledDepthBias);
    }

    static void clearCommonRectVector(uint32_t width, uint32_t height, const RenderRect *clearRects, uint32_t clearRectsCount, std::vector<VkClearRect> &rectVector) {
        rectVector.clear();

        if (clearRectsCount > 0) {
            for (uint32_t i = 0; i < clearRectsCount; i++) {
                VkClearRect clearRect;
                clearRect.rect.offset.x = clearRects[i].left;
                clearRect.rect.offset.y = clearRects[i].top;
                clearRect.rect.extent.width = clearRects[i].right - clearRects[i].left;
                clearRect.rect.extent.height = clearRects[i].bottom - clearRects[i].top;
                clearRect.baseArrayLayer = 0;
                clearRect.layerCount = 1;
                rectVector.emplace_back(clearRect);
            }
        }
        else {
            VkClearRect clearRect;
            clearRect.rect.offset.x = 0;
            clearRect.rect.offset.y = 0;
            clearRect.rect.extent.width = width;
            clearRect.rect.extent.height = height;
            clearRect.baseArrayLayer = 0;
            clearRect.layerCount = 1;
            rectVector.emplace_back(clearRect);
        }
    }

#if defined(__ANDROID__)
    // Log recorded CPU state, not an assertion that the GPU executed the commands.
    static bool logColorClearRecord(const VulkanFramebuffer *framebuffer, uint32_t attachmentIndex,
        const char *path, VkCommandBuffer commandBuffer, bool force = false) {
        static thread_local uint32_t records = 0;
        static thread_local bool loggedSwapchain = false;
        static thread_local bool loggedOffscreenMSAA = false;
        static thread_local bool loggedDraw = false;
        static thread_local bool loggedEmptyDraw = false;
        const VulkanTexture *texture = framebuffer->colorAttachments[attachmentIndex];
        const bool offscreenMSAA = texture->ownership && texture->desc.multisampling.sampleCount > 1;
        const bool draw = std::strcmp(path, "scissored-draw") == 0;
        const bool emptyDraw = std::strcmp(path, "scissored-draw-empty") == 0;
        const bool firstCategory = (texture->isSwapChainImage && !loggedSwapchain) ||
            (offscreenMSAA && !loggedOffscreenMSAA) || (!loggedDraw && draw) || (!loggedEmptyDraw && emptyDraw);
        if (records >= 16 && !firstCategory && !force) return false;
        if (records < 16) ++records;
        loggedSwapchain |= texture->isSwapChainImage;
        loggedOffscreenMSAA |= offscreenMSAA;
        loggedDraw |= draw;
        loggedEmptyDraw |= emptyDraw;
        char message[640];
        std::snprintf(message, sizeof(message),
            "Clear record: path=%s cb=%llx fb=%llx loadPass=%llx image=%llx boundView=%llx attachment=%u "
            "imageFormat=%u viewFormat=%u samples=%u usage=0x%X owned=%u swapchain=%u baseViewUsed=%u "
            "trackedLayout=%u trackedStages=0x%X fbExtent=%ux%u imageExtent=%ux%u mips=%u layers=%u",
            path, (unsigned long long)commandBuffer, (unsigned long long)framebuffer->vk,
            (unsigned long long)framebuffer->renderPass, (unsigned long long)texture->vk,
            (unsigned long long)framebuffer->colorAttachmentImageViews[attachmentIndex], attachmentIndex, unsigned(texture->imageFormat),
            unsigned(framebuffer->colorAttachmentFormats[attachmentIndex]), texture->desc.multisampling.sampleCount,
            texture->imageUsage, unsigned(texture->ownership), unsigned(texture->isSwapChainImage), unsigned(framebuffer->colorAttachmentUsesBaseView[attachmentIndex]),
            unsigned(texture->textureLayout), texture->barrierStages, framebuffer->width, framebuffer->height,
            texture->desc.width, texture->desc.height, texture->desc.mipLevels, texture->desc.arraySize);
        tooie_android_debug_log(message);
        return true;
    }

    static void rejectTransferClear(const VulkanFramebuffer *framebuffer, uint32_t attachmentIndex,
        const char *reason, VkCommandBuffer commandBuffer, VkFormatFeatureFlags formatFeatures = 0, bool formatQueried = false) {
        logColorClearRecord(framebuffer, attachmentIndex, "transfer-rejected", commandBuffer, true);
        char message[320];
        std::snprintf(message, sizeof(message),
            "Transfer clear rejected: reason=%s formatQueried=%u optimalFeatures=0x%X requiredFeature=0x%X",
            reason, unsigned(formatQueried), formatFeatures, VK_FORMAT_FEATURE_TRANSFER_DST_BIT);
        tooie_android_debug_log(message);
        std::fprintf(stderr, "%s\n", message);
#if defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK) && TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK
        const VulkanTexture *texture = framebuffer->colorAttachments[attachmentIndex];
        // A comparison may exercise ineligibility, never invalid Vulkan handles
        // or layouts. Keep those failures fatal in every build.
        if (texture->vk == VK_NULL_HANDLE || texture->imageView == VK_NULL_HANDLE ||
            framebuffer->vk == VK_NULL_HANDLE || framebuffer->renderPass == VK_NULL_HANDLE ||
            texture->textureLayout != RenderTextureLayout::COLOR_WRITE ||
            !(texture->barrierStages & RenderBarrierStage::GRAPHICS)) std::_Exit(EXIT_FAILURE);
        tooie_android_debug_log("Comparison only: explicit attachment-clear fallback enabled");
#else
        // Stop before beginning a fallback render pass or recording/submitting
        // the rejected clear. Default Android and diagnostic builds are strict.
        std::_Exit(EXIT_FAILURE);
#endif
    }
#endif

#if defined(__ANDROID__)
    static void rejectColorClearDraw(const VulkanFramebuffer *framebuffer, uint32_t index,
        VkCommandBuffer commandBuffer, const char *reason, const RenderRect *rectangles,
        uint32_t rectangleCount, uint32_t rectangleIndex) {
        logColorClearRecord(framebuffer, index, "draw-rejected", commandBuffer, true);
        char message[384];
        if (rectangles && rectangleIndex < rectangleCount) {
            const RenderRect &rect = rectangles[rectangleIndex];
            std::snprintf(message, sizeof(message),
                "Partial color clear rejected: reason=%s rectIndex=%u rectCount=%u originalRect=%d,%d..%d,%d fbExtent=%ux%u",
                reason, rectangleIndex, rectangleCount, rect.left, rect.top, rect.right, rect.bottom, framebuffer->width, framebuffer->height);
        }
        else {
            std::snprintf(message, sizeof(message),
                "Partial color clear rejected: reason=%s rectCount=%u originalRect=unavailable fbExtent=%ux%u",
                reason, rectangleCount, framebuffer->width, framebuffer->height);
        }
        tooie_android_debug_log(message);
        std::fprintf(stderr, "%s\n", message);
        std::_Exit(EXIT_FAILURE); // Never select vkCmdClearAttachments here.
    }

    VkPipeline VulkanFramebuffer::getColorClearDrawPipeline(uint32_t attachmentIndex) const {
        if (colorClearDrawPipelines.empty()) colorClearDrawPipelines.resize(colorAttachments.size(), VK_NULL_HANDLE);
        if (colorClearDrawPipelines[attachmentIndex] != VK_NULL_HANDLE) return colorClearDrawPipelines[attachmentIndex];
        auto requireSuccess = [](VkResult result, const char *operation) {
            if (result == VK_SUCCESS) return;
            char message[256];
            std::snprintf(message, sizeof(message), "Partial color clear setup failed: operation=%s result=%d", operation, int(result));
            tooie_android_debug_log(message);
            std::fprintf(stderr, "%s\n", message);
            std::_Exit(EXIT_FAILURE);
        };
        if (colorClearDrawLayout == VK_NULL_HANDLE) {
            VkPushConstantRange range = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderColor) + sizeof(RenderRect) };
            VkPipelineLayoutCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            info.pushConstantRangeCount = 1;
            info.pPushConstantRanges = &range;
            requireSuccess(vkCreatePipelineLayout(device->vk, &info, nullptr, &colorClearDrawLayout), "vkCreatePipelineLayout");
        }
        VkShaderModule modules[2] = {};
        VkShaderModuleCreateInfo moduleInfo = {};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = sizeof(ColorClearVertex);
        moduleInfo.pCode = ColorClearVertex;
        requireSuccess(vkCreateShaderModule(device->vk, &moduleInfo, nullptr, &modules[0]), "vertex module");
        // Patch the generated, validated output Location decoration, not colour
        // channel order. RGBA floats are converted by the actual attachment view.
        std::vector<uint32_t> fragment(std::begin(ColorClearFragment), std::end(ColorClearFragment));
        fragment[ColorClearFragmentLocationWord] = attachmentIndex;
        moduleInfo.codeSize = fragment.size() * sizeof(uint32_t);
        moduleInfo.pCode = fragment.data();
        requireSuccess(vkCreateShaderModule(device->vk, &moduleInfo, nullptr, &modules[1]), "fragment module");
        VkPipelineShaderStageCreateInfo stages[2] = {};
        for (uint32_t i = 0; i < 2; ++i) {
            stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].stage = i == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[i].module = modules[i];
            stages[i].pName = "main";
        }
        VkPipelineVertexInputStateCreateInfo vertex = {};
        vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo assembly = {};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport = {};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster = {};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VkSampleCountFlagBits(colorAttachments[attachmentIndex]->desc.multisampling.sampleCount);
        // Null sample mask means every sample; no alpha-to-coverage or shading
        // masks. Integer scissor edges cover all samples of each selected pixel.
        VkPipelineDepthStencilStateCreateInfo depth = {};
        depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(colorAttachments.size());
        blendAttachments[attachmentIndex].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend = {};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = uint32_t(blendAttachments.size());
        blend.pAttachments = blendAttachments.data();
        const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
        VkPipelineDynamicStateCreateInfo dynamic = {};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = 3;
        dynamic.pDynamicStates = dynamicStates;
        VkGraphicsPipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertex;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &depth;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = colorClearDrawLayout;
        info.renderPass = renderPass; // Exact framebuffer pass, including depth/MRT.
        VkResult result = vkCreateGraphicsPipelines(device->vk, VK_NULL_HANDLE, 1, &info, nullptr, &colorClearDrawPipelines[attachmentIndex]);
        for (VkShaderModule module : modules) vkDestroyShaderModule(device->vk, module, nullptr);
        requireSuccess(result, "vkCreateGraphicsPipelines");
        return colorClearDrawPipelines[attachmentIndex];
    }

    void VulkanCommandList::clearColorDraw(uint32_t attachmentIndex, RenderColor colorValue,
        const RenderRect *clearRects, uint32_t clearRectsCount) {
        const VulkanTexture *texture = targetFramebuffer->colorAttachments[attachmentIndex];
        const auto reject = [&](const char *reason, uint32_t rectangleIndex = 0) {
            rejectColorClearDraw(targetFramebuffer, attachmentIndex, vk, reason, clearRects, clearRectsCount, rectangleIndex);
        };
        // Validate before ending a pass or recording any command for this clear.
        if (!clearRects || !clearRectsCount) reject("missing-rectangles");
        if (texture->vk == VK_NULL_HANDLE) reject("null-image");
        if (targetFramebuffer->colorAttachmentImageViews[attachmentIndex] == VK_NULL_HANDLE) reject("null-attachment-view");
        if (targetFramebuffer->vk == VK_NULL_HANDLE) reject("null-framebuffer");
        if (targetFramebuffer->renderPass == VK_NULL_HANDLE) reject("null-render-pass");
        if (!targetFramebuffer->width || !targetFramebuffer->height) reject("empty-framebuffer");
        if (targetFramebuffer->width > INT32_MAX || targetFramebuffer->height > INT32_MAX) reject("framebuffer-extent-unrepresentable");
        if (texture->desc.dimension != RenderTextureDimension::TEXTURE_2D) reject("not-2d");
        if (!(texture->imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) reject("missing-color-attachment-usage");
        if (attachmentIndex >= ColorClearMaxAttachments) reject("attachment-location-unsupported");
        if (targetFramebuffer->colorAttachments.size() > 1 && !queue->device->independentBlendSupported) reject("independent-blend-unsupported");
        const VkFormat format = targetFramebuffer->colorAttachmentFormats[attachmentIndex];
        switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_R16G16B16A16_UNORM: case VK_FORMAT_R16G16B16A16_SFLOAT:
        case VK_FORMAT_R32G32B32A32_SFLOAT: break;
        default: reject("unsupported-draw-format");
        }
        VkFormatProperties properties = {};
        vkGetPhysicalDeviceFormatProperties(queue->device->physicalDevice, format, &properties);
        const VkFormatFeatureFlags features = texture->desc.textureArrangement == RenderTextureArrangement::ROW_MAJOR ?
            properties.linearTilingFeatures : properties.optimalTilingFeatures;
        if (!(features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) reject("format-color-attachment-unsupported");
        for (uint32_t i = 0; i < targetFramebuffer->colorAttachments.size(); ++i) {
            const VulkanTexture *attachment = targetFramebuffer->colorAttachments[i];
            if (attachment->vk == VK_NULL_HANDLE) reject("null-image");
            if (targetFramebuffer->colorAttachmentImageViews[i] == VK_NULL_HANDLE) reject("null-attachment-view");
            if (attachment->textureLayout != RenderTextureLayout::COLOR_WRITE) reject("layout-not-color-write");
            if (!(attachment->barrierStages & RenderBarrierStage::GRAPHICS)) reject("missing-graphics-stage");
            if (attachment->desc.multisampling.sampleCount != texture->desc.multisampling.sampleCount) reject("attachment-sample-count-mismatch");
        }
        if (targetFramebuffer->depthAttachment) {
            const VulkanTexture *depth = targetFramebuffer->depthAttachment;
            if (depth->vk == VK_NULL_HANDLE) reject("null-depth-image");
            const RenderTextureLayout expected = targetFramebuffer->depthAttachmentReadOnly ? RenderTextureLayout::DEPTH_READ : RenderTextureLayout::DEPTH_WRITE;
            if (depth->textureLayout != expected) reject("depth-layout-mismatch");
            if (!(depth->barrierStages & RenderBarrierStage::GRAPHICS)) reject("depth-missing-graphics-stage");
            if (depth->desc.multisampling.sampleCount != texture->desc.multisampling.sampleCount) reject("depth-sample-count-mismatch");
        }
        // FillRect retains the original game rectangle, while target sizing uses
        // its scissor intersection. Oversized/negative coordinates are valid;
        // intersect them with the actual framebuffer/view domain. Validate the
        // entire batch before touching commands, including empty intersections.
        thread_local std::vector<RenderRect> clippedRectangles;
        clippedRectangles.clear();
        clippedRectangles.reserve(clearRectsCount);
        bool hasIntersection = false;
        for (uint32_t i = 0; i < clearRectsCount; ++i) {
            const RenderRect &rect = clearRects[i];
            if (rect.right <= rect.left || rect.bottom <= rect.top) reject("malformed-rectangle", i);
            // No subtraction or unsigned conversion until the clipped bounds
            // are nonempty and nonnegative; INT32_MIN/MAX inputs cannot wrap.
            RenderRect clipped(std::max(rect.left, 0), std::max(rect.top, 0),
                std::min(rect.right, int32_t(targetFramebuffer->width)), std::min(rect.bottom, int32_t(targetFramebuffer->height)));
            hasIntersection |= !clipped.isEmpty();
            clippedRectangles.emplace_back(clipped);
        }
        const VkPipeline pipeline = hasIntersection ? targetFramebuffer->getColorClearDrawPipeline(attachmentIndex) : VK_NULL_HANDLE;
        const bool record = logColorClearRecord(targetFramebuffer, attachmentIndex,
            hasIntersection ? "scissored-draw" : "scissored-draw-empty", vk);
        if (record) {
            const auto &view = targetFramebuffer->colorAttachmentViewDescs[attachmentIndex];
            for (uint32_t i = 0; i < clearRectsCount; ++i) {
                const RenderRect &original = clearRects[i], &clipped = clippedRectangles[i];
                char message[448];
                std::snprintf(message, sizeof(message),
                    "Clear draw: attachment=%u rectIndex=%u originalRect=%d,%d..%d,%d rect=%d,%d..%d,%d intersection=%s "
                    "fbExtent=%ux%u viewMip=%u viewLayer=%u rgba=%g,%g,%g,%g LOAD/STORE no layout change",
                    attachmentIndex, i, original.left, original.top, original.right, original.bottom,
                    clipped.left, clipped.top, clipped.right, clipped.bottom, clipped.isEmpty() ? "empty-no-draw" : "nonempty",
                    targetFramebuffer->width, targetFramebuffer->height, view.mipSlice, view.arrayIndex,
                    colorValue.r, colorValue.g, colorValue.b, colorValue.a);
                tooie_android_debug_log(message);
            }
        }
        // A valid fully outside batch is a no-op: no pipeline creation, pass
        // restart, barrier, draw, transfer clear or caller-state changes.
        if (!hasIntersection) return;
        // Preserve previous attachment writes across STORE -> LOAD, including
        // every other colour and depth/stencil target. No image layout change:
        // the draw uses COLOR_ATTACHMENT_OPTIMAL and the actual bound view.
        endActiveRenderPass();
        VkMemoryBarrier preserve = {};
        preserve.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        preserve.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        preserve.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        const VkPipelineStageFlags attachmentStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        vkCmdPipelineBarrier(vk, attachmentStages, attachmentStages, 0, 1, &preserve, 0, nullptr, 0, nullptr);
        checkActiveRenderPass(); // LOAD/STORE, never LOAD_OP_CLEAR.
        vkCmdBindPipeline(vk, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        const VkViewport viewport = { 0.0f, 0.0f, float(targetFramebuffer->width), float(targetFramebuffer->height), 0.0f, 1.0f };
        vkCmdSetViewport(vk, 0, 1, &viewport);
        vkCmdSetDepthBias(vk, 0.0f, 0.0f, 0.0f);
        for (const RenderRect &rect : clippedRectangles) {
            if (rect.isEmpty()) continue;
            const VkRect2D scissor = { { rect.left, rect.top }, { uint32_t(rect.right - rect.left), uint32_t(rect.bottom - rect.top) } };
            struct ClearConstants { RenderColor color; RenderRect rectangle; } constants = { colorValue, rect };
            static_assert(sizeof(ClearConstants) == 32, "Clear shader push constant ABI");
            vkCmdPushConstants(vk, targetFramebuffer->colorClearDrawLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants), &constants);
            vkCmdSetScissor(vk, 0, 1, &scissor);
            vkCmdDraw(vk, 3, 1, 0, 0);
        }
        // Callers (FramebufferRenderer/UI) cache these bindings. Restore without
        // altering our caller-state cache. Descriptors and vertex/index buffers
        // were never rebound, and the caller's pipeline layout pointer is intact.
        if (graphicsPipeline != VK_NULL_HANDLE) vkCmdBindPipeline(vk, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        if (!graphicsViewports.empty()) vkCmdSetViewport(vk, 0, uint32_t(graphicsViewports.size()), graphicsViewports.data());
        if (!graphicsScissors.empty()) vkCmdSetScissor(vk, 0, uint32_t(graphicsScissors.size()), graphicsScissors.data());
        if (graphicsDepthBiasSet) vkCmdSetDepthBias(vk, graphicsDepthBias[0], graphicsDepthBias[1], graphicsDepthBias[2]);
        for (const GraphicsPushConstantWrite &write : graphicsPushConstantWrites) {
            vkCmdPushConstants(vk, write.layout, write.stages, write.offset, uint32_t(write.data.size()), write.data.data());
        }
    }
#endif

    void VulkanCommandList::clearColor(uint32_t attachmentIndex, RenderColor colorValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
        assert(targetFramebuffer != nullptr);
        assert(attachmentIndex < targetFramebuffer->colorAttachments.size());
        assert((clearRectsCount == 0) || (clearRects != nullptr));

#if defined(__ANDROID__)
#if !defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) && !(defined(TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK) && TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK)
        // Explicit rectangles are a separate, view-local operation. Never widen
        // them into a whole-image transfer clear or select the old clear fallback.
        if (clearRectsCount > 0) {
            clearColorDraw(attachmentIndex, colorValue, clearRects, clearRectsCount);
            return;
        }
#endif
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) && !TOOIE_VULKAN_TRANSFER_CLEAR_TEST
        // Retain the failed Portal load-op experiment for controlled comparison.
        // Its D submission contains a memory barrier AND a CLEAR render pass.
        if (clearRectsCount == 0) {
            const bool recordClear = logColorClearRecord(targetFramebuffer, attachmentIndex, "load-op-control", vk);
            endActiveRenderPass();

            // A full clear can restart a pass after earlier draws. Preserve
            // attachment writes for the LOAD operations on the other targets.
            VkMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            const VkPipelineStageFlags attachmentStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            vkCmdPipelineBarrier(vk, attachmentStages, attachmentStages, 0,
                1, &barrier, 0, nullptr, 0, nullptr);

#if defined(TOOIE_VULKAN_CLEAR_BARRIER_TEST) && TOOIE_VULKAN_CLEAR_BARRIER_TEST
            // Isolate the additional D memory barrier, without starting a pass.
            tooie_android_debug_log("Clear record: load-op D barrier only; no render pass or clear");
            return;
#endif
            std::vector<VkClearValue> clearValues(targetFramebuffer->colorAttachments.size());
            clearValues[attachmentIndex].color.float32[0] = colorValue.r;
            clearValues[attachmentIndex].color.float32[1] = colorValue.g;
            clearValues[attachmentIndex].color.float32[2] = colorValue.b;
            clearValues[attachmentIndex].color.float32[3] = colorValue.a;

            VkRenderPassBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass = targetFramebuffer->colorClearRenderPasses[attachmentIndex];
            beginInfo.framebuffer = targetFramebuffer->vk;
            beginInfo.renderArea.extent = { targetFramebuffer->width, targetFramebuffer->height };
            beginInfo.clearValueCount = uint32_t(clearValues.size());
            beginInfo.pClearValues = clearValues.data();
            char beginRecord[160];
            std::snprintf(beginRecord, sizeof(beginRecord),
                "Clear record: begin CLEAR pass=%llx compatibleLoadPass=%llx values=%u selected=%u",
                (unsigned long long)beginInfo.renderPass, (unsigned long long)targetFramebuffer->renderPass,
                beginInfo.clearValueCount, attachmentIndex);
            if (recordClear) tooie_android_debug_log(beginRecord);
            vkCmdBeginRenderPass(vk, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
            activeRenderPass = beginInfo.renderPass;
            return;
        }
#else
        const VulkanTexture *colorTexture = targetFramebuffer->colorAttachments[attachmentIndex];
        // The proposal deliberately covers the launcher's plain RGBA/BGRA UNORM
        // images only. A framebuffer clear is not a whole-image clear for a
        // smaller render area, attachment view, or integer/reinterpreted format.
        const char *rejection = nullptr;
        VkFormatProperties properties = {};
        bool formatQueried = false;
        if (clearRectsCount != 0) rejection = "partial-rectangles";
        else if (!targetFramebuffer->colorAttachmentUsesBaseView[attachmentIndex]) rejection = "explicit-attachment-view";
        else if (colorTexture->desc.dimension != RenderTextureDimension::TEXTURE_2D) rejection = "not-2d";
        else if (colorTexture->desc.textureArrangement == RenderTextureArrangement::ROW_MAJOR) rejection = "linear-tiling";
        else if (colorTexture->desc.mipLevels != 1) rejection = "multiple-mip-levels";
        else if (colorTexture->desc.arraySize != 1) rejection = "multiple-array-layers";
        else if (colorTexture->desc.width != targetFramebuffer->width ||
            colorTexture->desc.height != targetFramebuffer->height) rejection = "framebuffer-extent-mismatch";
        else if (!(colorTexture->imageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) rejection = "missing-transfer-dst-usage";
        else if (colorTexture->imageFormat != targetFramebuffer->colorAttachmentFormats[attachmentIndex]) rejection = "attachment-format-mismatch";
        else if (colorTexture->imageFormat != VK_FORMAT_R8G8B8A8_UNORM &&
            colorTexture->imageFormat != VK_FORMAT_B8G8R8A8_UNORM) rejection = "unsupported-clear-format";
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) && TOOIE_VULKAN_TRANSFER_CLEAR_TEST
        else if (colorTexture->ownership) rejection = "legacy-requires-external-image";
        else if (colorTexture->desc.multisampling.sampleCount != 1) rejection = "legacy-requires-single-sample";
#endif
        else if (colorTexture->vk == VK_NULL_HANDLE) rejection = "null-image";
        else if (colorTexture->imageView == VK_NULL_HANDLE) rejection = "null-image-view";
        else if (targetFramebuffer->vk == VK_NULL_HANDLE) rejection = "null-framebuffer";
        else if (targetFramebuffer->renderPass == VK_NULL_HANDLE) rejection = "null-render-pass";
        else if (colorTexture->textureLayout != RenderTextureLayout::COLOR_WRITE) rejection = "layout-not-color-write";
        else if (!(colorTexture->barrierStages & RenderBarrierStage::GRAPHICS)) rejection = "missing-graphics-stage";
        if (rejection == nullptr) {
            vkGetPhysicalDeviceFormatProperties(queue->device->physicalDevice, colorTexture->imageFormat, &properties);
            formatQueried = true;
            if (!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) rejection = "format-transfer-dst-unsupported";
        }
        if (rejection == nullptr) {
            const bool recordClear = logColorClearRecord(targetFramebuffer, attachmentIndex, "transfer", vk);
            endActiveRenderPass(); // Transfer commands must be outside a render pass.

            VkImageSubresourceRange range = {};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.levelCount = 1;
            range.layerCount = 1;

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = colorTexture->vk;
            barrier.subresourceRange = range;
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = toImageLayout(colorTexture->textureLayout);
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            // Splitting a pass must also preserve any OTHER attachments that
            // will be LOADed when drawing resumes. The launcher has one color
            // target and no depth, so its recorded path needs no such barrier.
            VkMemoryBarrier preserve = {};
            preserve.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            preserve.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            preserve.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            const bool preserveOthers = (targetFramebuffer->colorAttachments.size() > 1) || targetFramebuffer->depthAttachment;
            const VkPipelineStageFlags otherStages = preserveOthers ?
                (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) : 0;
            vkCmdPipelineBarrier(vk, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | otherStages,
                VK_PIPELINE_STAGE_TRANSFER_BIT | otherStages, 0, preserveOthers ? 1 : 0,
                preserveOthers ? &preserve : nullptr, 0, nullptr, 1, &barrier);

            VkClearColorValue clearValue = {};
            clearValue.float32[0] = colorValue.r;
            clearValue.float32[1] = colorValue.g;
            clearValue.float32[2] = colorValue.b;
            clearValue.float32[3] = colorValue.a;
            // Vulkan clears every sample; vkCmdClearColorImage has no 1x-only
            // restriction. The Portal's MSAA transfer path is still unverified.
            vkCmdClearColorImage(vk, colorTexture->vk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);

            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            vkCmdPipelineBarrier(vk, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
            // COLOR_WRITE/GRAPHICS tracking is unchanged: both transitions were
            // scoped to this clear and the image is restored before returning.
            if (recordClear) tooie_android_debug_log("Clear record: COLOR->TRANSFER_DST; clear mip=0 layer=0; TRANSFER_DST->COLOR; no clear render pass");
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) && TOOIE_VULKAN_TRANSFER_CLEAR_TEST
            tooie_android_debug_log("DIAG D: swapchain vkCmdClearColorImage recorded");
#endif
            return;
        }
        rejectTransferClear(targetFramebuffer, attachmentIndex, rejection, vk, properties.optimalTilingFeatures, formatQueried);
        logColorClearRecord(targetFramebuffer, attachmentIndex, "attachment-fallback", vk, true);
#endif
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) && !TOOIE_VULKAN_TRANSFER_CLEAR_TEST
        // Full clears returned through the explicitly selected load-op control.
        // A partial clear must not silently reach the old attachment command.
        rejectTransferClear(targetFramebuffer, attachmentIndex, "partial-rectangles", vk);
#endif
#endif
        checkActiveRenderPass();

        thread_local std::vector<VkClearRect> rectVector;
        clearCommonRectVector(targetFramebuffer->getWidth(), targetFramebuffer->getHeight(), clearRects, clearRectsCount, rectVector);

        VkClearAttachment attachment = {};
        auto &rgba = attachment.clearValue.color.float32;
        rgba[0] = colorValue.r;
        rgba[1] = colorValue.g;
        rgba[2] = colorValue.b;
        rgba[3] = colorValue.a;
        attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        attachment.colorAttachment = attachmentIndex;
#if defined(__ANDROID__) && defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) && TOOIE_VULKAN_TRANSFER_CLEAR_TEST
        // Preserve the transfer-only isolation diagnostic without skipping
        // offscreen UI clears in the normal launcher path.
        tooie_android_debug_log("DIAG: non-swapchain attachment clear skipped");
#else
        vkCmdClearAttachments(vk, 1, &attachment, uint32_t(rectVector.size()), rectVector.data());
#endif
    }

    void VulkanCommandList::clearDepthStencil(bool clearDepth, bool clearStencil, float depthValue, uint32_t stencilValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
        assert(targetFramebuffer != nullptr);
        assert((clearRectsCount == 0) || (clearRects != nullptr));

        checkActiveRenderPass();

        thread_local std::vector<VkClearRect> rectVector;
        clearCommonRectVector(targetFramebuffer->getWidth(), targetFramebuffer->getHeight(), clearRects, clearRectsCount, rectVector);

        VkClearAttachment attachment = {};
        attachment.clearValue.depthStencil.depth = depthValue;
        attachment.clearValue.depthStencil.stencil = stencilValue;

        if (clearDepth) {
            attachment.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        if (clearStencil) {
            attachment.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        vkCmdClearAttachments(vk, 1, &attachment, uint32_t(rectVector.size()), rectVector.data());
    }

    void VulkanCommandList::copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) {
        endActiveRenderPass();

        assert(dstBuffer.ref != nullptr);
        assert(srcBuffer.ref != nullptr);

        const VulkanBuffer *interfaceDstBuffer = static_cast<const VulkanBuffer *>(dstBuffer.ref);
        const VulkanBuffer *interfaceSrcBuffer = static_cast<const VulkanBuffer *>(srcBuffer.ref);
        VkBufferCopy bufferCopy = {};
        bufferCopy.dstOffset = dstBuffer.offset;
        bufferCopy.srcOffset = srcBuffer.offset;
        bufferCopy.size = size;
        vkCmdCopyBuffer(vk, interfaceSrcBuffer->vk, interfaceDstBuffer->vk, 1, &bufferCopy);
    }

    void VulkanCommandList::copyTextureRegion(const RenderTextureCopyLocation &dstLocation, const RenderTextureCopyLocation &srcLocation, uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox *srcBox) {
        endActiveRenderPass();
        
        assert(dstLocation.type != RenderTextureCopyType::UNKNOWN);
        assert(srcLocation.type != RenderTextureCopyType::UNKNOWN);

        const VulkanTexture *dstTexture = static_cast<const VulkanTexture *>(dstLocation.texture);
        const VulkanTexture *srcTexture = static_cast<const VulkanTexture *>(srcLocation.texture);
        const VulkanBuffer *dstBuffer = static_cast<const VulkanBuffer *>(dstLocation.buffer);
        const VulkanBuffer *srcBuffer = static_cast<const VulkanBuffer *>(srcLocation.buffer);
        if ((dstLocation.type == RenderTextureCopyType::SUBRESOURCE) && (srcLocation.type == RenderTextureCopyType::PLACED_FOOTPRINT)) {
            assert(dstTexture != nullptr);
            assert(srcBuffer != nullptr);

            const uint32_t blockWidth = RenderFormatBlockWidth(dstTexture->desc.format);
            VkBufferImageCopy imageCopy = {};
            imageCopy.bufferOffset = srcLocation.placedFootprint.offset;
            imageCopy.bufferRowLength = ((srcLocation.placedFootprint.rowWidth + blockWidth - 1) / blockWidth) * blockWidth;
            imageCopy.bufferImageHeight = ((srcLocation.placedFootprint.height + blockWidth - 1) / blockWidth) * blockWidth;
            imageCopy.imageSubresource.aspectMask = toAspectFlags(dstTexture->desc.format, dstTexture->desc.flags);
            imageCopy.imageSubresource.baseArrayLayer = dstLocation.subresource.arrayIndex;
            imageCopy.imageSubresource.layerCount = 1;
            imageCopy.imageSubresource.mipLevel = dstLocation.subresource.mipLevel;
            imageCopy.imageOffset.x = dstX;
            imageCopy.imageOffset.y = dstY;
            imageCopy.imageOffset.z = dstZ;
            imageCopy.imageExtent.width = srcLocation.placedFootprint.width;
            imageCopy.imageExtent.height = srcLocation.placedFootprint.height;
            imageCopy.imageExtent.depth = srcLocation.placedFootprint.depth;
            vkCmdCopyBufferToImage(vk, srcBuffer->vk, dstTexture->vk, toImageLayout(dstTexture->textureLayout), 1, &imageCopy);
        }
        else {
            VkImageCopy imageCopy = {};
            imageCopy.srcSubresource.aspectMask = toAspectFlags(srcTexture->desc.format, srcTexture->desc.flags);
            imageCopy.srcSubresource.baseArrayLayer = srcLocation.subresource.arrayIndex;
            imageCopy.srcSubresource.layerCount = 1;
            imageCopy.srcSubresource.mipLevel = srcLocation.subresource.mipLevel;
            imageCopy.dstSubresource.aspectMask = toAspectFlags(dstTexture->desc.format, dstTexture->desc.flags);
            imageCopy.dstSubresource.baseArrayLayer = dstLocation.subresource.arrayIndex;
            imageCopy.dstSubresource.layerCount = 1;
            imageCopy.dstSubresource.mipLevel = dstLocation.subresource.mipLevel;
            imageCopy.dstOffset.x = dstX;
            imageCopy.dstOffset.y = dstY;
            imageCopy.dstOffset.z = dstZ;

            if (srcBox != nullptr) {
                imageCopy.srcOffset.x = srcBox->left;
                imageCopy.srcOffset.y = srcBox->top;
                imageCopy.srcOffset.z = srcBox->front;
                imageCopy.extent.width = srcBox->right - srcBox->left;
                imageCopy.extent.height = srcBox->bottom - srcBox->top;
                imageCopy.extent.depth = srcBox->back - srcBox->front;
            }
            else {
                imageCopy.srcOffset.x = 0;
                imageCopy.srcOffset.y = 0;
                imageCopy.srcOffset.z = 0;
                imageCopy.extent.width = srcTexture->desc.width;
                imageCopy.extent.height = srcTexture->desc.height;
                imageCopy.extent.depth = srcTexture->desc.depth;
            }

            vkCmdCopyImage(vk, srcTexture->vk, toImageLayout(srcTexture->textureLayout), dstTexture->vk, toImageLayout(dstTexture->textureLayout), 1, &imageCopy);
        }
    }

    void VulkanCommandList::copyBuffer(const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) {
        endActiveRenderPass();

        assert(dstBuffer != nullptr);
        assert(srcBuffer != nullptr);
        
        const VulkanBuffer *interfaceDstBuffer = static_cast<const VulkanBuffer *>(dstBuffer);
        const VulkanBuffer *interfaceSrcBuffer = static_cast<const VulkanBuffer *>(srcBuffer);
        VkBufferCopy bufferCopy = {};
        bufferCopy.dstOffset = 0;
        bufferCopy.srcOffset = 0;
        bufferCopy.size = interfaceDstBuffer->desc.size;
        vkCmdCopyBuffer(vk, interfaceSrcBuffer->vk, interfaceDstBuffer->vk, 1, &bufferCopy);
    }

    void VulkanCommandList::copyTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
        endActiveRenderPass();

        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);

        thread_local std::vector<VkImageCopy> imageCopies;
        imageCopies.clear();

        const VulkanTexture *dst = static_cast<const VulkanTexture *>(dstTexture);
        const VulkanTexture *src = static_cast<const VulkanTexture *>(srcTexture);
        VkImageLayout srcLayout = toImageLayout(src->textureLayout);
        VkImageLayout dstLayout = toImageLayout(dst->textureLayout);
        VkImageCopy imageCopy = {};
        imageCopy.srcSubresource.aspectMask = toAspectFlags(src->desc.format, src->desc.flags);
        imageCopy.srcSubresource.baseArrayLayer = 0;
        imageCopy.srcSubresource.layerCount = 1;
        imageCopy.dstSubresource.aspectMask = toAspectFlags(dst->desc.format, dst->desc.flags);
        imageCopy.dstSubresource.baseArrayLayer = 0;
        imageCopy.dstSubresource.layerCount = 1;
        imageCopy.extent.width = uint32_t(dst->desc.width);
        imageCopy.extent.height = dst->desc.height;
        imageCopy.extent.depth = dst->desc.depth;

        assert(dst->desc.mipLevels > 0);
        assert(src->desc.mipLevels == dst->desc.mipLevels);

        for (uint32_t i = 0; i < dst->desc.mipLevels; i++) {
            imageCopy.srcSubresource.mipLevel = i;
            imageCopy.dstSubresource.mipLevel = i;
            imageCopies.emplace_back(imageCopy);
        }

        vkCmdCopyImage(vk, src->vk, srcLayout, dst->vk, dstLayout, uint32_t(imageCopies.size()), imageCopies.data());
    }

    void VulkanCommandList::resolveTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
        resolveTextureRegion(dstTexture, 0, 0, srcTexture, nullptr, RenderResolveMode::AVERAGE);
    }

    void VulkanCommandList::resolveTextureRegion(const RenderTexture *dstTexture, uint32_t dstX, uint32_t dstY, const RenderTexture *srcTexture, const RenderRect *srcRect, RenderResolveMode resolveMode) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);
        assert(resolveMode == RenderResolveMode::AVERAGE && "Vulkan only supports AVERAGE resolve mode.");

        thread_local std::vector<VkImageResolve> imageResolves;
        imageResolves.clear();

        const VulkanTexture *dst = static_cast<const VulkanTexture *>(dstTexture);
        const VulkanTexture *src = static_cast<const VulkanTexture *>(srcTexture);
        VkImageLayout srcLayout = toImageLayout(src->textureLayout);
        VkImageLayout dstLayout = toImageLayout(dst->textureLayout);
        VkImageResolve imageResolve = {};
        imageResolve.srcSubresource.aspectMask = toAspectFlags(src->desc.format, src->desc.flags);
        imageResolve.srcSubresource.baseArrayLayer = 0;
        imageResolve.srcSubresource.layerCount = 1;
        imageResolve.dstOffset.x = dstX;
        imageResolve.dstOffset.y = dstY;
        imageResolve.dstSubresource.aspectMask = toAspectFlags(dst->desc.format, dst->desc.flags);
        imageResolve.dstSubresource.baseArrayLayer = 0;
        imageResolve.dstSubresource.layerCount = 1;
        imageResolve.extent.depth = dst->desc.depth;

        if (srcRect != nullptr) {
            imageResolve.srcOffset.x = srcRect->left;
            imageResolve.srcOffset.y = srcRect->top;
            imageResolve.extent.width = (srcRect->right - srcRect->left);
            imageResolve.extent.height = (srcRect->bottom - srcRect->top);
        }
        else {
            imageResolve.extent.width = uint32_t(dst->desc.width);
            imageResolve.extent.height = dst->desc.height;
        }

        assert(dst->desc.mipLevels > 0);
        assert(src->desc.mipLevels == dst->desc.mipLevels);

        for (uint32_t i = 0; i < dst->desc.mipLevels; i++) {
            imageResolve.srcSubresource.mipLevel = i;
            imageResolve.dstSubresource.mipLevel = i;
            imageResolves.emplace_back(imageResolve);
        }

        vkCmdResolveImage(vk, src->vk, srcLayout, dst->vk, dstLayout, uint32_t(imageResolves.size()), imageResolves.data());
    }
    
    void VulkanCommandList::buildBottomLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, const RenderBottomLevelASBuildInfo &buildInfo) {
        assert(dstAccelerationStructure != nullptr);
        assert(scratchBuffer.ref != nullptr);

        const VulkanAccelerationStructure *interfaceAccelerationStructure = static_cast<const VulkanAccelerationStructure *>(dstAccelerationStructure);
        assert(interfaceAccelerationStructure->type == RenderAccelerationStructureType::BOTTOM_LEVEL);

        const VulkanBuffer *interfaceScratchBuffer = static_cast<const VulkanBuffer *>(scratchBuffer.ref);
        assert((interfaceScratchBuffer->desc.flags & RenderBufferFlag::ACCELERATION_STRUCTURE_SCRATCH) && "Scratch buffer must be allowed.");

        VkBufferDeviceAddressInfo scratchAddressInfo = {};
        scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        scratchAddressInfo.buffer = interfaceScratchBuffer->vk;

        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
        buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildGeometryInfo.flags = toRTASBuildFlags(buildInfo.preferFastBuild, buildInfo.preferFastTrace);
        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.dstAccelerationStructure = interfaceAccelerationStructure->vk;
        buildGeometryInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(queue->device->vk, &scratchAddressInfo) + scratchBuffer.offset;
        buildGeometryInfo.pGeometries = reinterpret_cast<const VkAccelerationStructureGeometryKHR *>(buildInfo.buildData.data());
        buildGeometryInfo.geometryCount = buildInfo.meshCount;

        VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {};
        buildRangeInfo.primitiveCount = buildInfo.primitiveCount;
        buildRangeInfo.primitiveOffset = 0;
        buildRangeInfo.firstVertex = 0;
        buildRangeInfo.transformOffset = 0;

        VkAccelerationStructureBuildRangeInfoKHR *buildRangeInfoPtr = &buildRangeInfo;
        vkCmdBuildAccelerationStructuresKHR(vk, 1, &buildGeometryInfo, &buildRangeInfoPtr);
    }

    void VulkanCommandList::buildTopLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, RenderBufferReference instancesBuffer, const RenderTopLevelASBuildInfo &buildInfo) {
        assert(dstAccelerationStructure != nullptr);
        assert(scratchBuffer.ref != nullptr);
        assert(instancesBuffer.ref != nullptr);

        const VulkanAccelerationStructure *interfaceAccelerationStructure = static_cast<const VulkanAccelerationStructure *>(dstAccelerationStructure);
        assert(interfaceAccelerationStructure->type == RenderAccelerationStructureType::TOP_LEVEL);

        const VulkanBuffer *interfaceScratchBuffer = static_cast<const VulkanBuffer *>(scratchBuffer.ref);
        assert((interfaceScratchBuffer->desc.flags & RenderBufferFlag::ACCELERATION_STRUCTURE_SCRATCH) && "Scratch buffer must be allowed.");

        VkBufferDeviceAddressInfo scratchAddressInfo = {};
        scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        scratchAddressInfo.buffer = interfaceScratchBuffer->vk;

        const VulkanBuffer *interfaceInstancesBuffer = static_cast<const VulkanBuffer *>(instancesBuffer.ref);
        assert((interfaceInstancesBuffer->desc.flags & RenderBufferFlag::ACCELERATION_STRUCTURE_INPUT) && "Acceleration structure input must be allowed.");

        VkBufferDeviceAddressInfo instancesAddressInfo = {};
        instancesAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        instancesAddressInfo.buffer = interfaceInstancesBuffer->vk;

        VkAccelerationStructureGeometryKHR topGeometry = {};
        topGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        topGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;

        VkAccelerationStructureGeometryInstancesDataKHR &instancesData = topGeometry.geometry.instances;
        instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instancesData.data.deviceAddress = vkGetBufferDeviceAddress(queue->device->vk, &instancesAddressInfo) + instancesBuffer.offset;

        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
        buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.dstAccelerationStructure = interfaceAccelerationStructure->vk;
        buildGeometryInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(queue->device->vk, &scratchAddressInfo) + scratchBuffer.offset;
        buildGeometryInfo.pGeometries = &topGeometry;
        buildGeometryInfo.geometryCount = 1;

        VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {};
        buildRangeInfo.primitiveCount = buildInfo.instanceCount;
        buildRangeInfo.primitiveOffset = 0;
        buildRangeInfo.firstVertex = 0;
        buildRangeInfo.transformOffset = 0;

        VkAccelerationStructureBuildRangeInfoKHR *buildRangeInfoPtr = &buildRangeInfo;
        vkCmdBuildAccelerationStructuresKHR(vk, 1, &buildGeometryInfo, &buildRangeInfoPtr);
    }

    void VulkanCommandList::discardTexture(const RenderTexture* texture) {
        // Not required in Vulkan.
    }

    void VulkanCommandList::resetQueryPool(const RenderQueryPool *queryPool, uint32_t queryFirstIndex, uint32_t queryCount) {
        assert(queryPool != nullptr);

        const VulkanQueryPool *interfaceQueryPool = static_cast<const VulkanQueryPool *>(queryPool);
        vkCmdResetQueryPool(vk, interfaceQueryPool->vk, queryFirstIndex, queryCount);
    }

    void VulkanCommandList::writeTimestamp(const RenderQueryPool *queryPool, uint32_t queryIndex) {
        assert(queryPool != nullptr);

        const VulkanQueryPool *interfaceQueryPool = static_cast<const VulkanQueryPool *>(queryPool);
        vkCmdWriteTimestamp(vk, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, interfaceQueryPool->vk, queryIndex);
    }

    void VulkanCommandList::checkActiveRenderPass() {
        assert(targetFramebuffer != nullptr);
        
        if (activeRenderPass == VK_NULL_HANDLE) {
            VkRenderPassBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass = targetFramebuffer->renderPass;
            beginInfo.framebuffer = targetFramebuffer->vk;
            beginInfo.renderArea.extent.width = targetFramebuffer->width;
            beginInfo.renderArea.extent.height = targetFramebuffer->height;
            vkCmdBeginRenderPass(vk, &beginInfo, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);
            activeRenderPass = targetFramebuffer->renderPass;
        }
    }

    void VulkanCommandList::endActiveRenderPass() {
        if (activeRenderPass != VK_NULL_HANDLE) {
            vkCmdEndRenderPass(vk);
            activeRenderPass = VK_NULL_HANDLE;
        }
    }

    void VulkanCommandList::setDescriptorSet(VkPipelineBindPoint bindPoint, const VulkanPipelineLayout *pipelineLayout, const RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        assert(pipelineLayout != nullptr);
        assert(descriptorSet != nullptr);
        assert(setIndex < pipelineLayout->descriptorSetLayouts.size());

        const VulkanDescriptorSet *interfaceSet = static_cast<const VulkanDescriptorSet *>(descriptorSet);
        vkCmdBindDescriptorSets(vk, bindPoint, pipelineLayout->vk, setIndex, 1, &interfaceSet->vk, 0, nullptr);
    }

    // VulkanCommandFence

    VulkanCommandFence::VulkanCommandFence(VulkanDevice *device) {
        assert(device != nullptr);

        this->device = device;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        VkResult res = vkCreateFence(device->vk, &fenceInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateFence failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanCommandFence::~VulkanCommandFence() {
        if (vk != VK_NULL_HANDLE) {
            vkDestroyFence(device->vk, vk, nullptr);
        }
    }

    // VulkanCommandSemaphore

    VulkanCommandSemaphore::VulkanCommandSemaphore(VulkanDevice *device) {
        assert(device != nullptr);

        this->device = device;

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkResult res = vkCreateSemaphore(device->vk, &semaphoreInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateSemaphore failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanCommandSemaphore::~VulkanCommandSemaphore() {
        if (vk != VK_NULL_HANDLE) {
            vkDestroySemaphore(device->vk, vk, nullptr);
        }
    }

    // VulkanCommandQueue

    VulkanCommandQueue::VulkanCommandQueue(VulkanDevice *device, RenderCommandListType type) {
        assert(device != nullptr);
        assert(type != RenderCommandListType::UNKNOWN);

        this->device = device;
        this->type = type;

        familyIndex = device->queueFamilyIndices[toFamilyIndex(type)];
        device->queueFamilies[familyIndex].add(this);
    }

    VulkanCommandQueue::~VulkanCommandQueue() {
        device->queueFamilies[familyIndex].remove(this);
    }

    std::unique_ptr<RenderCommandList> VulkanCommandQueue::createCommandList() {
        return std::make_unique<VulkanCommandList>(this);
    }

    std::unique_ptr<RenderSwapChain> VulkanCommandQueue::createSwapChain(const RenderSwapChainDesc &desc) {
        return std::make_unique<VulkanSwapChain>(this, desc);
    }

    void VulkanCommandQueue::executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount, RenderCommandSemaphore **signalSemaphores, uint32_t signalSemaphoreCount, RenderCommandFence *signalFence) {
        assert(commandLists != nullptr);
        assert(commandListCount > 0);

        thread_local std::vector<VkSemaphore> waitSemaphoreVector;
        thread_local std::vector<VkSemaphore> signalSemaphoreVector;
        thread_local std::vector<VkCommandBuffer> commandBuffers;
        waitSemaphoreVector.clear();
        signalSemaphoreVector.clear();
        commandBuffers.clear();

        for (uint32_t i = 0; i < waitSemaphoreCount; i++) {
            VulkanCommandSemaphore *interfaceSemaphore = static_cast<VulkanCommandSemaphore *>(waitSemaphores[i]);
            waitSemaphoreVector.emplace_back(interfaceSemaphore->vk);
        }

        for (uint32_t i = 0; i < signalSemaphoreCount; i++) {
            VulkanCommandSemaphore *interfaceSemaphore = static_cast<VulkanCommandSemaphore *>(signalSemaphores[i]);
            signalSemaphoreVector.emplace_back(interfaceSemaphore->vk);
        }

        for (uint32_t i = 0; i < commandListCount; i++) {
            assert(commandLists[i] != nullptr);

            const VulkanCommandList *interfaceCommandList = static_cast<const VulkanCommandList *>(commandLists[i]);
            commandBuffers.emplace_back(interfaceCommandList->vk);
        }

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pCommandBuffers = commandBuffers.data();
        submitInfo.commandBufferCount = uint32_t(commandBuffers.size());

#if defined(__ANDROID__)
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
#else
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif
        // Vulkan requires one stage mask for each wait semaphore, not a single
        // scalar when a submission waits on multiple semaphores.
        const std::vector<VkPipelineStageFlags> waitStages(waitSemaphoreVector.size(), waitStage);
        if (!waitSemaphoreVector.empty()) {
            submitInfo.pWaitSemaphores = waitSemaphoreVector.data();
            submitInfo.waitSemaphoreCount = uint32_t(waitSemaphoreVector.size());
            submitInfo.pWaitDstStageMask = waitStages.data();
        }

        if (!signalSemaphoreVector.empty()) {
            submitInfo.pSignalSemaphores = signalSemaphoreVector.data();
            submitInfo.signalSemaphoreCount = uint32_t(signalSemaphoreVector.size());
        }

        VkFence submitFence = VK_NULL_HANDLE;
        if (signalFence != nullptr) {
            VulkanCommandFence *interfaceFence = static_cast<VulkanCommandFence *>(signalFence);
            submitFence = interfaceFence->vk;
        }

        VkResult res;
        {
            const std::scoped_lock queueLock(*queue->mutex);
            res = vkQueueSubmit(queue->vk, 1, &submitInfo, submitFence);
        }

#if defined(__ANDROID__)
        {
            static thread_local int submissionLogs = 0;
            if (submissionLogs++ < 8 || res != VK_SUCCESS) {
                char message[128];
                std::snprintf(message, sizeof(message),
                    "Plume: vkQueueSubmit result=%d", (int)res);
                tooie_android_debug_log(message);
            }
        }
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkQueueSubmit failed with error code 0x%X.\n", res);
#if defined(__ANDROID__)
            std::_Exit(EXIT_FAILURE);
#endif
            return;
        }
    }
    
    void VulkanCommandQueue::waitForCommandFence(RenderCommandFence *fence) {
        assert(fence != nullptr);

        VulkanCommandFence *interfaceFence = static_cast<VulkanCommandFence *>(fence);
#if defined(__ANDROID__)
        constexpr uint64_t fenceTimeoutNs = 5000000000ULL;
        VkResult res = vkWaitForFences(
            device->vk, 1, &interfaceFence->vk, VK_TRUE, fenceTimeoutNs);
#else
        VkResult res = vkWaitForFences(
            device->vk, 1, &interfaceFence->vk, VK_TRUE, UINT64_MAX);
#endif
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkWaitForFences failed with error code 0x%X.\n", res);
#if defined(__ANDROID__)
            char message[128];
            std::snprintf(message, sizeof(message),
                "Plume: Vulkan fence timeout/error result=%d", (int)res);
            tooie_android_debug_log(message);
            std::_Exit(EXIT_FAILURE);
#endif
            return;
        }

        vkResetFences(device->vk, 1, &interfaceFence->vk);
    }

    // VulkanPool

    VulkanPool::VulkanPool(VulkanDevice *device, const RenderPoolDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        VmaAllocationCreateInfo memoryInfo = {};
        switch (desc.heapType) {
        case RenderHeapType::DEFAULT:
            memoryInfo.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case RenderHeapType::UPLOAD:
            memoryInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;
        case RenderHeapType::READBACK:
            memoryInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            break;
        default:
            assert(false && "Unknown heap type.");
            break;
        }

        uint32_t memoryTypeIndex = 0;
        VkResult res = vmaFindMemoryTypeIndex(device->allocator, UINT32_MAX, &memoryInfo, &memoryTypeIndex);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vmaFindMemoryTypeIndex failed with error code 0x%X.\n", res);
            return;
        }

        VmaPoolCreateInfo createInfo = {};
        createInfo.memoryTypeIndex = memoryTypeIndex;
        createInfo.minBlockCount = desc.minBlockCount;
        createInfo.maxBlockCount = desc.maxBlockCount;
        createInfo.flags |= desc.useLinearAlgorithm ? VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT : 0;

        res = vmaCreatePool(device->allocator, &createInfo, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vmaCreatePool failed with error code 0x%X.\n", res);
            return;
        }
    }

    VulkanPool::~VulkanPool() {
        if (vk != VK_NULL_HANDLE) {
            vmaDestroyPool(device->allocator, vk);
        }
    }

    std::unique_ptr<RenderBuffer> VulkanPool::createBuffer(const RenderBufferDesc &desc) {
        return std::make_unique<VulkanBuffer>(device, this, desc);
    }

    std::unique_ptr<RenderTexture> VulkanPool::createTexture(const RenderTextureDesc &desc) {
        return std::make_unique<VulkanTexture>(device, this, desc);
    }
    
    // VulkanQueueFamily

    void VulkanQueueFamily::add(VulkanCommandQueue *virtualQueue) {
        assert(virtualQueue != nullptr);

        // Insert virtual queue into the queue with the least amount of virtual queues.
        uint32_t queueIndex = 0;
        uint32_t lowestCount = UINT_MAX;
        for (uint32_t i = 0; i < queues.size(); i++) {
            uint32_t virtualQueueCount = uint32_t(queues[i].virtualQueues.size());
            if (virtualQueueCount < lowestCount) {
                queueIndex = i;
                lowestCount = virtualQueueCount;
            }
        }

        if (queues[queueIndex].mutex == nullptr) {
            queues[queueIndex].mutex = std::make_unique<std::mutex>();
        }

        virtualQueue->queue = &queues[queueIndex];
        virtualQueue->queueIndex = queueIndex;
        queues[queueIndex].virtualQueues.insert(virtualQueue);
    }

    void VulkanQueueFamily::remove(VulkanCommandQueue *virtualQueue) {
        assert(virtualQueue != nullptr);

        queues[virtualQueue->queueIndex].virtualQueues.erase(virtualQueue);
    }

    // VulkanDevice
    
    VulkanDevice::VulkanDevice(VulkanInterface *renderInterface, const std::string &preferredDeviceName) {
        assert(renderInterface != nullptr);

        this->renderInterface = renderInterface;

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(renderInterface->instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            fprintf(stderr, "Unable to find devices that support Vulkan.\n");
            return;
        }
        
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(renderInterface->instance, &deviceCount, physicalDevices.data());

        uint32_t currentDeviceTypeScore = 0;
        uint32_t deviceTypeScoreTable[] = {
            0,  // VK_PHYSICAL_DEVICE_TYPE_OTHER
            3,  // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
            4,  // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            2,  // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
            1   // VK_PHYSICAL_DEVICE_TYPE_CPU
        };

        for (uint32_t i = 0; i < deviceCount; i++) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);

            uint32_t deviceTypeIndex = deviceProperties.deviceType;
            if (deviceTypeIndex > 4) {
                continue;
            }

            std::string deviceName(deviceProperties.deviceName);
            uint32_t deviceTypeScore = deviceTypeScoreTable[deviceTypeIndex];
            bool preferDeviceTypeScore = (deviceTypeScore > currentDeviceTypeScore);
            bool preferUserChoice = preferredDeviceName == deviceName;
            bool preferOption = preferDeviceTypeScore || preferUserChoice;
            if (preferOption) {
                physicalDevice = physicalDevices[i];
                description.name = deviceName;
                description.type = toDeviceType(deviceProperties.deviceType);
                description.driverVersion = deviceProperties.driverVersion;
                description.vendor = RenderDeviceVendor(deviceProperties.vendorID);
                currentDeviceTypeScore = deviceTypeScore;

                if (preferUserChoice) {
                    break;
                }
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            fprintf(stderr, "Unable to find a device with the required features.\n");
            return;
        }

        // Check for extensions.
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

        std::unordered_set<std::string> missingRequiredExtensions = RequiredDeviceExtensions;
        std::unordered_set<std::string> supportedOptionalExtensions;
#   if DLSS_ENABLED
        const std::unordered_set<std::string> dlssExtensions = DLSS::getRequiredDeviceExtensionsVulkan(this);
#   endif
        for (uint32_t i = 0; i < extensionCount; i++) {
            const std::string extensionName(availableExtensions[i].extensionName);
            missingRequiredExtensions.erase(extensionName);

            if (OptionalDeviceExtensions.find(extensionName) != OptionalDeviceExtensions.end()) {
                supportedOptionalExtensions.insert(extensionName);
            }
#       if DLSS_ENABLED
            else if (dlssExtensions.find(extensionName) != dlssExtensions.end()) {
                supportedOptionalExtensions.insert(extensionName);
            }
#       endif
        }
        
        if (!missingRequiredExtensions.empty()) {
            for (const std::string &extension : missingRequiredExtensions) {
                fprintf(stderr, "Missing required extension: %s.\n", extension.c_str());
            }

            fprintf(stderr, "Unable to create device. Required extensions are missing.\n");
            return;
        }

        // Store properties.
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        // Check for supported features.
        void *featuresChain = nullptr;
        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
        const bool descriptorIndexingFound = supportedOptionalExtensions.find(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) != supportedOptionalExtensions.end()
#if defined(__ANDROID__)
            // Descriptor indexing is core in Vulkan 1.2; the extension name is
            // optional on newer devices. Query and enable core features too.
            || (renderInterface->appInfo.apiVersion >= VK_API_VERSION_1_2 && physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_2)
#endif
            ;
        if (descriptorIndexingFound) {
            indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            featuresChain = &indexingFeatures;
        }

        VkPhysicalDeviceScalarBlockLayoutFeatures layoutFeatures = {};
        const bool scalarBlockLayoutFound = supportedOptionalExtensions.find(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME) != supportedOptionalExtensions.end();
        if (scalarBlockLayoutFound) {
            layoutFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
            layoutFeatures.pNext = featuresChain;
            featuresChain = &layoutFeatures;
        }

        VkPhysicalDevicePresentIdFeaturesKHR presentIdFeatures = {};
        VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeatures = {};
        const bool presentWaitFound = supportedOptionalExtensions.find(VK_KHR_PRESENT_ID_EXTENSION_NAME) != supportedOptionalExtensions.end() && supportedOptionalExtensions.find(VK_KHR_PRESENT_WAIT_EXTENSION_NAME) != supportedOptionalExtensions.end();
        if (presentWaitFound) {
            presentIdFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
            presentIdFeatures.pNext = featuresChain;
            featuresChain = &presentIdFeatures;

            presentWaitFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
            presentWaitFeatures.pNext = featuresChain;
            featuresChain = &presentWaitFeatures;
        }

        VkPhysicalDeviceRobustness2FeaturesEXT robustnessFeatures = {};
        const bool robustnessFound = supportedOptionalExtensions.find(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME) != supportedOptionalExtensions.end();
        if (robustnessFound) {
            robustnessFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
            robustnessFeatures.pNext = featuresChain;
            featuresChain = &robustnessFeatures;
        }

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {};
        const bool bufferDeviceAddressFound = supportedOptionalExtensions.find(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) != supportedOptionalExtensions.end();
        if (bufferDeviceAddressFound) {
            bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            bufferDeviceAddressFeatures.pNext = featuresChain;
            featuresChain = &bufferDeviceAddressFeatures;
        }

        VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures = {};
        const bool portabilityFound = supportedOptionalExtensions.find(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) != supportedOptionalExtensions.end();
        if (portabilityFound) {
            portabilityFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
            portabilityFeatures.pNext = featuresChain;
            featuresChain = &portabilityFeatures;
        }

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
        const bool rayTracingFound = (supportedOptionalExtensions.find(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) != supportedOptionalExtensions.end()) && (supportedOptionalExtensions.find(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) != supportedOptionalExtensions.end());
        if (rayTracingFound) {
            rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            rayTracingPipelineFeatures.pNext = featuresChain;
            featuresChain = &rayTracingPipelineFeatures;

            accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            accelerationStructureFeatures.pNext = featuresChain;
            featuresChain = &accelerationStructureFeatures;
        }

        VkPhysicalDeviceFeatures2 deviceFeatures = {};
        deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext = featuresChain;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

#if defined(__ANDROID__)
        if (descriptorIndexingFound) {
            descriptorIndexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
            VkPhysicalDeviceProperties2 indexingProperties = {};
            indexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            indexingProperties.pNext = &descriptorIndexingProperties;
            vkGetPhysicalDeviceProperties2(physicalDevice, &indexingProperties);
        }
#endif

        // Check for properties.
        if (rayTracingFound) {
            rtPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

            VkPhysicalDeviceProperties2 deviceProperties2 = {};
            deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProperties2.pNext = &rtPipelineProperties;
            vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);
        }

        const bool sampleLocationsFound = supportedOptionalExtensions.find(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME) != supportedOptionalExtensions.end();
        if (sampleLocationsFound) {
            sampleLocationProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT;

            VkPhysicalDeviceProperties2 deviceProperties2 = {};
            deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProperties2.pNext = &sampleLocationProperties;
            vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);
        }

        // Build the device creation chain.
        void *createDeviceChain = nullptr;
        const bool rayTracingSupported = rayTracingPipelineFeatures.rayTracingPipeline && accelerationStructureFeatures.accelerationStructure;
        if (rayTracingSupported) {
            rayTracingPipelineFeatures.pNext = createDeviceChain;
            createDeviceChain = &rayTracingPipelineFeatures;

            accelerationStructureFeatures.pNext = createDeviceChain;
            createDeviceChain = &accelerationStructureFeatures;
        }

        const bool descriptorIndexingSupported = indexingFeatures.descriptorBindingPartiallyBound && indexingFeatures.descriptorBindingVariableDescriptorCount && indexingFeatures.runtimeDescriptorArray;
        if (descriptorIndexingSupported) {
            indexingFeatures.pNext = createDeviceChain;
            createDeviceChain = &indexingFeatures;
        }

        const bool scalarBlockLayoutSupported = layoutFeatures.scalarBlockLayout;
        if (scalarBlockLayoutSupported) {
            layoutFeatures.pNext = createDeviceChain;
            createDeviceChain = &layoutFeatures;
        }

        const bool presentWaitSupported = presentIdFeatures.presentId && presentWaitFeatures.presentWait;
        if (presentWaitSupported) {
            presentIdFeatures.pNext = createDeviceChain;
            createDeviceChain = &presentIdFeatures;

            presentWaitFeatures.pNext = createDeviceChain;
            createDeviceChain = &presentWaitFeatures;
        }

#if defined(__ANDROID__)
        independentBlendSupported = deviceFeatures.features.independentBlend;
#endif
        nullDescriptorSupported = robustnessFeatures.nullDescriptor;
        if (nullDescriptorSupported) {
            robustnessFeatures.pNext = createDeviceChain;
            createDeviceChain = &robustnessFeatures;
        }

        const bool bufferDeviceAddressSupported = bufferDeviceAddressFeatures.bufferDeviceAddress;
        if (bufferDeviceAddressSupported) {
            bufferDeviceAddressFeatures.pNext = createDeviceChain;
            createDeviceChain = &bufferDeviceAddressFeatures;
        }
        
        if (portabilityFound) {
            portabilityFeatures.pNext = createDeviceChain;
            createDeviceChain = &portabilityFeatures;
        }

        // Retrieve the information for the queue families.
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        std::vector<bool> queueFamilyUsed(queueFamilyCount, false);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

        auto pickFamilyQueue = [&](RenderCommandListType type, VkQueueFlags flags) {
            uint32_t familyIndex = 0;
            uint32_t familySetBits = sizeof(uint32_t) * 8;
            uint32_t familyQueueCount = 0;
            bool familyUsed = false;
            for (uint32_t i = 0; i < queueFamilyCount; i++) {
                const VkQueueFamilyProperties &props = queueFamilyProperties[i];

                // The family queue flags must contain all the flags required by the command list type.
                if ((props.queueFlags & flags) != flags) {
                    continue;
                }

                // Prefer picking the queues with the least amount of bits set that match the mask we're looking for.
                // If the queue families have matching capabilities but one is already used, prefer the unused one.
                uint32_t setBits = numberOfSetBits(props.queueFlags);
                bool used = queueFamilyUsed[i];
                if ((setBits < familySetBits) || ((setBits == familySetBits) && ((props.queueCount > familyQueueCount) || (familyUsed && !used)))) {
                    familyIndex = i;
                    familySetBits = setBits;
                    familyQueueCount = props.queueCount;
                    familyUsed = used;
                }
            }

            queueFamilyIndices[toFamilyIndex(type)] = familyIndex;
            queueFamilyUsed[familyIndex] = true;
        };

        // Pick the family queues for each type of command list.
        pickFamilyQueue(RenderCommandListType::DIRECT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
        pickFamilyQueue(RenderCommandListType::COMPUTE, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
        pickFamilyQueue(RenderCommandListType::COPY, VK_QUEUE_TRANSFER_BIT);

        // Create the struct to store the virtual queues.
        queueFamilies.resize(queueFamilyCount);

        // Create the logical device with the desired family queues.
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::vector<float> queuePriorities(MaxQueuesPerFamilyCount, 1.0f);
        queueCreateInfos.reserve(queueFamilyCount);
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilyUsed[i]) {
                VkDeviceQueueCreateInfo queueCreateInfo = {};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueCount = std::min(queueFamilyProperties[i].queueCount, MaxQueuesPerFamilyCount);
                queueCreateInfo.queueFamilyIndex = i;
                queueCreateInfo.pQueuePriorities = queuePriorities.data();
                queueCreateInfos.emplace_back(queueCreateInfo);
                queueFamilies[i].queues.resize(queueCreateInfo.queueCount);
            }
        }

        std::vector<const char *> enabledExtensions;
        for (const std::string &extension : RequiredDeviceExtensions) {
            enabledExtensions.push_back(extension.c_str());
        }

        for (const std::string &extension : supportedOptionalExtensions) {
            enabledExtensions.push_back(extension.c_str());
        }

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = createDeviceChain;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
        createInfo.enabledExtensionCount = uint32_t(enabledExtensions.size());
        createInfo.pEnabledFeatures = &deviceFeatures.features;

        VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &vk);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkCreateDevice failed with error code 0x%X.\n", res);
            return;
        }

#if defined(__ANDROID__)
        if (descriptorIndexingSupported) {
            descriptorIndexingFeatures = indexingFeatures;
            descriptorIndexingFeatures.pNext = nullptr;
        }
        char indexingRecord[384];
        std::snprintf(indexingRecord, sizeof(indexingRecord), "Descriptor indexing discovery: pid=%d tid=%d instanceApi=0x%X deviceApi=0x%X extension=%u queried=%u enabled=%u",
            int(getpid()), int(gettid()), renderInterface->appInfo.apiVersion, physicalDeviceProperties.apiVersion,
            unsigned(supportedOptionalExtensions.count(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) != 0), unsigned(descriptorIndexingFound), unsigned(descriptorIndexingSupported));
        tooie_android_debug_log(indexingRecord);
#if defined(TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE) && TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE
        // Opt-in CPU allocation experiment before VMA, RT64 resources, worker
        // threads or GPU submissions. Normal rendering never uses these pools.
        std::snprintf(indexingRecord, sizeof(indexingRecord), "Descriptor allocation probe start: pid=%d tid=%d early isolated experiment; no raster/GPU work",
            int(getpid()), int(gettid()));
        tooie_android_debug_log(indexingRecord);
        const bool probeSucceeded = runVulkanDescriptorAllocationProbe(vk, descriptorIndexingFeatures);
        if (probeSucceeded) {
            // Compare the same shape through the production constructors after
            // the independent raw cases. Their normal fatal guards still apply.
            std::snprintf(indexingRecord, sizeof(indexingRecord), "Descriptor allocation probe: pid=%d tid=%d production-path control begin", int(getpid()), int(gettid()));
            tooie_android_debug_log(indexingRecord);
            {
                RenderDescriptorRange range(RenderDescriptorRangeType::TEXTURE, 0, 8192, nullptr);
                const auto productionControl = createDescriptorSet(RenderDescriptorSetDesc(&range, 1, true, 1));
            }
            std::snprintf(indexingRecord, sizeof(indexingRecord), "Descriptor allocation probe: pid=%d tid=%d production-path control retired", int(getpid()), int(gettid()));
            tooie_android_debug_log(indexingRecord);
        }
        vkDestroyDevice(vk, nullptr); vk = VK_NULL_HANDLE;
        std::snprintf(indexingRecord, sizeof(indexingRecord), "Descriptor allocation probe complete: pid=%d tid=%d allSucceeded=%u exit=%u; intentional diagnostic stop",
            int(getpid()), int(gettid()), unsigned(probeSucceeded), probeSucceeded ? 0u : 1u);
        tooie_android_debug_log(indexingRecord); std::fprintf(stderr, "%s\n", indexingRecord);
        std::_Exit(probeSucceeded ? EXIT_SUCCESS : EXIT_FAILURE);
#endif
#endif

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            for (uint32_t j = 0; j < queueFamilies[i].queues.size(); j++) {
                vkGetDeviceQueue(vk, i, j, &queueFamilies[i].queues[j].vk);
            }
        }

        VmaVulkanFunctions vmaFunctions = {};
        vmaFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vmaFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        vmaFunctions.vkAllocateMemory = vkAllocateMemory;
        vmaFunctions.vkBindBufferMemory = vkBindBufferMemory;
        vmaFunctions.vkBindImageMemory = vkBindImageMemory;
        vmaFunctions.vkCreateBuffer = vkCreateBuffer;
        vmaFunctions.vkCreateImage = vkCreateImage;
        vmaFunctions.vkDestroyBuffer = vkDestroyBuffer;
        vmaFunctions.vkDestroyImage = vkDestroyImage;
        vmaFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vmaFunctions.vkFreeMemory = vkFreeMemory;
        vmaFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vmaFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        vmaFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vmaFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vmaFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
        vmaFunctions.vkMapMemory = vkMapMemory;
        vmaFunctions.vkUnmapMemory = vkUnmapMemory;
        vmaFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.flags |= bufferDeviceAddressSupported ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0;
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = vk;
        allocatorInfo.pVulkanFunctions = &vmaFunctions;
        allocatorInfo.instance = renderInterface->instance;
        allocatorInfo.vulkanApiVersion = renderInterface->appInfo.apiVersion;

        res = vmaCreateAllocator(&allocatorInfo, &allocator);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vmaCreateAllocator failed with error code 0x%X.\n", res);
            release();
            return;
        }

        // Find the biggest device local memory available on the device.
        VkDeviceSize memoryHeapSize = 0;
        const VkPhysicalDeviceMemoryProperties *memoryProps = nullptr;
        vmaGetMemoryProperties(allocator, &memoryProps);

        constexpr VkMemoryPropertyFlags uploadHeapPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        bool hasHostVisibleDeviceLocalMemory = false;
        for (uint32_t i = 0; i < memoryProps->memoryTypeCount; i++) {
            if ((memoryProps->memoryTypes[i].propertyFlags & uploadHeapPropertyFlags) == uploadHeapPropertyFlags) {
                hasHostVisibleDeviceLocalMemory = true;
            }
        }

        for (uint32_t i = 0; i < memoryProps->memoryHeapCount; i++) {
            if (memoryProps->memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                memoryHeapSize = std::max(memoryProps->memoryHeaps[i].size, memoryHeapSize);
            }
        }

        // Fill description.
        description.dedicatedVideoMemory = memoryHeapSize;

        // Fill capabilities.
        capabilities.geometryShader = deviceFeatures.features.geometryShader;
        capabilities.raytracing = rayTracingSupported;
        capabilities.raytracingStateUpdate = false;
        capabilities.sampleLocations = (sampleLocationProperties.sampleLocationSampleCounts != 0);
        capabilities.resolveRegion = true;
        capabilities.resolveModes = false;
        capabilities.descriptorIndexing = descriptorIndexingSupported;
        capabilities.scalarBlockLayout = scalarBlockLayoutSupported;
        capabilities.bufferDeviceAddress = bufferDeviceAddressSupported;
        capabilities.samplerMirrorClampToEdge = supportedOptionalExtensions.find(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME) != supportedOptionalExtensions.end();
        capabilities.presentWait = presentWaitSupported;
        capabilities.displayTiming = supportedOptionalExtensions.find(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME) != supportedOptionalExtensions.end();
        capabilities.maxTextureSize = physicalDeviceProperties.limits.maxImageDimension2D;
        capabilities.preferHDR = memoryHeapSize > (512 * 1024 * 1024);
        capabilities.dynamicDepthBias = true;
        capabilities.queryPools = true;
        capabilities.uma = (description.type == RenderDeviceType::INTEGRATED) && hasHostVisibleDeviceLocalMemory;
        capabilities.gpuUploadHeap = capabilities.uma;

#   if defined(__APPLE__)
        // MoltenVK supports triangle fans but does so via compute shaders to translate to lists, since it has to
        // support all cases including indirect draw. This results in renderpass restarts that can harm performance,
        // so force disable native triangle fan support and rely on the game to emulate fans if needed.
        capabilities.triangleFan = false;
#   else
        capabilities.triangleFan = true;
#   endif

        // Fill Vulkan-only capabilities.
        loadStoreOpNoneSupported = supportedOptionalExtensions.find(VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME) != supportedOptionalExtensions.end();

        if (!nullDescriptorSupported) {
            nullBuffer = createBuffer(RenderBufferDesc::DefaultBuffer(16, RenderBufferFlag::VERTEX));
        }
    }

    VulkanDevice::~VulkanDevice() {
        release();
    }

    std::unique_ptr<RenderDescriptorSet> VulkanDevice::createDescriptorSet(const RenderDescriptorSetDesc &desc) {
        return std::make_unique<VulkanDescriptorSet>(this, desc);
    }

    std::unique_ptr<RenderShader> VulkanDevice::createShader(const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) {
        return std::make_unique<VulkanShader>(this, data, size, entryPointName, format);
    }

    std::unique_ptr<RenderSampler> VulkanDevice::createSampler(const RenderSamplerDesc &desc) {
        return std::make_unique<VulkanSampler>(this, desc);
    }

    std::unique_ptr<RenderPipeline> VulkanDevice::createComputePipeline(const RenderComputePipelineDesc &desc) {
        return std::make_unique<VulkanComputePipeline>(this, desc);
    }

    std::unique_ptr<RenderPipeline> VulkanDevice::createGraphicsPipeline(const RenderGraphicsPipelineDesc &desc) {
        return std::make_unique<VulkanGraphicsPipeline>(this, desc);
    }

    std::unique_ptr<RenderPipeline> VulkanDevice::createRaytracingPipeline(const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline) {
        return std::make_unique<VulkanRaytracingPipeline>(this, desc, previousPipeline);
    }

    std::unique_ptr<RenderCommandQueue> VulkanDevice::createCommandQueue(RenderCommandListType type) {
        return std::make_unique<VulkanCommandQueue>(this, type);
    }

    std::unique_ptr<RenderBuffer> VulkanDevice::createBuffer(const RenderBufferDesc &desc) {
        return std::make_unique<VulkanBuffer>(this, nullptr, desc);
    }

    std::unique_ptr<RenderTexture> VulkanDevice::createTexture(const RenderTextureDesc &desc) {
        return std::make_unique<VulkanTexture>(this, nullptr, desc);
    }

    std::unique_ptr<RenderAccelerationStructure> VulkanDevice::createAccelerationStructure(const RenderAccelerationStructureDesc &desc) {
        return std::make_unique<VulkanAccelerationStructure>(this, desc);
    }

    std::unique_ptr<RenderPool> VulkanDevice::createPool(const RenderPoolDesc &desc) {
        return std::make_unique<VulkanPool>(this, desc);
    }

    std::unique_ptr<RenderPipelineLayout> VulkanDevice::createPipelineLayout(const RenderPipelineLayoutDesc &desc) {
        return std::make_unique<VulkanPipelineLayout>(this, desc);
    }

    std::unique_ptr<RenderCommandFence> VulkanDevice::createCommandFence() {
        return std::make_unique<VulkanCommandFence>(this);
    }

    std::unique_ptr<RenderCommandSemaphore> VulkanDevice::createCommandSemaphore() {
        return std::make_unique<VulkanCommandSemaphore>(this);
    }

    std::unique_ptr<RenderFramebuffer> VulkanDevice::createFramebuffer(const RenderFramebufferDesc &desc) {
        return std::make_unique<VulkanFramebuffer>(this, desc);
    }

    std::unique_ptr<RenderQueryPool> VulkanDevice::createQueryPool(uint32_t queryCount) {
        return std::make_unique<VulkanQueryPool>(this, queryCount);
    }

    void VulkanDevice::setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo &buildInfo, const RenderBottomLevelASMesh *meshes, uint32_t meshCount, bool preferFastBuild, bool preferFastTrace) {
        assert(meshes != nullptr);
        assert(meshCount > 0);

        uint32_t primitiveCount = 0;
        thread_local std::vector<uint32_t> geometryPrimitiveCounts;
        geometryPrimitiveCounts.resize(meshCount);

        buildInfo.buildData.resize(sizeof(VkAccelerationStructureGeometryKHR) * meshCount);
        VkAccelerationStructureGeometryKHR *geometries = reinterpret_cast<VkAccelerationStructureGeometryKHR *>(buildInfo.buildData.data());
        for (uint32_t i = 0; i < meshCount; i++) {
            const RenderBottomLevelASMesh &mesh = meshes[i];
            VkAccelerationStructureGeometryKHR &geometry = geometries[i];
            geometry = {};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
            geometry.flags |= mesh.isOpaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;

            const VulkanBuffer *interfaceVertexBuffer = static_cast<const VulkanBuffer *>(mesh.vertexBuffer.ref);
            const VulkanBuffer *interfaceIndexBuffer = static_cast<const VulkanBuffer *>(mesh.indexBuffer.ref);
            assert((interfaceIndexBuffer == nullptr) || ((interfaceIndexBuffer->desc.flags & RenderBufferFlag::ACCELERATION_STRUCTURE_INPUT) && "Acceleration structure input allowed on index buffer."));
            assert((interfaceVertexBuffer == nullptr) || ((interfaceVertexBuffer->desc.flags & RenderBufferFlag::ACCELERATION_STRUCTURE_INPUT) && "Acceleration structure input allowed on vertex buffer."));

            VkAccelerationStructureGeometryTrianglesDataKHR &triangles = geometry.geometry.triangles;
            triangles = {};
            triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.vertexFormat = toVk(mesh.vertexFormat);
            triangles.vertexStride = mesh.vertexStride;
            triangles.maxVertex = mesh.vertexCount - 1;

            if (interfaceVertexBuffer != nullptr) {
                VkBufferDeviceAddressInfo vertexAddressInfo = {};
                vertexAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                vertexAddressInfo.buffer = interfaceVertexBuffer->vk;
                triangles.vertexData.deviceAddress = vkGetBufferDeviceAddress(vk, &vertexAddressInfo) + mesh.vertexBuffer.offset;
            }

            if (interfaceIndexBuffer != nullptr) {
                triangles.indexType = toIndexType(mesh.indexFormat);

                VkBufferDeviceAddressInfo indexAddressInfo = {};
                indexAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                indexAddressInfo.buffer = interfaceIndexBuffer->vk;
                triangles.indexData.deviceAddress = vkGetBufferDeviceAddress(vk, &indexAddressInfo) + mesh.indexBuffer.offset;
                geometryPrimitiveCounts[i] = mesh.indexCount / 3;
            }
            else {
                triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
                geometryPrimitiveCounts[i] = mesh.vertexCount / 3;
            }

            primitiveCount += geometryPrimitiveCounts[i];
        }

        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
        buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildGeometryInfo.flags = toRTASBuildFlags(preferFastBuild, preferFastTrace);
        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.pGeometries = geometries;
        buildGeometryInfo.geometryCount = meshCount;

        VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {};
        buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(vk, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, geometryPrimitiveCounts.data(), &buildSizesInfo);

        buildInfo.meshCount = meshCount;
        buildInfo.primitiveCount = primitiveCount;
        buildInfo.preferFastBuild = preferFastBuild;
        buildInfo.preferFastTrace = preferFastTrace;
        buildInfo.scratchSize = roundUp(buildSizesInfo.buildScratchSize, AccelerationStructureBufferAlignment);
        buildInfo.accelerationStructureSize = roundUp(buildSizesInfo.accelerationStructureSize, AccelerationStructureBufferAlignment);
    }

    void VulkanDevice::setTopLevelASBuildInfo(RenderTopLevelASBuildInfo &buildInfo, const RenderTopLevelASInstance *instances, uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace) {
        assert(instances != nullptr);
        assert(instanceCount > 0);

        // Build the instance data to be uploaded.
        buildInfo.instancesBufferData.resize(sizeof(VkAccelerationStructureInstanceKHR) * instanceCount, 0);
        VkAccelerationStructureInstanceKHR *bufferInstances = reinterpret_cast<VkAccelerationStructureInstanceKHR *>(buildInfo.instancesBufferData.data());
        for (uint32_t i = 0; i < instanceCount; i++) {
            const RenderTopLevelASInstance &instance = instances[i];
            const VulkanBuffer *interfaceBottomLevelAS = static_cast<const VulkanBuffer *>(instance.bottomLevelAS.ref);
            assert(interfaceBottomLevelAS != nullptr);

            VkAccelerationStructureInstanceKHR &bufferInstance = bufferInstances[i];
            bufferInstance.instanceCustomIndex = instance.instanceID;
            bufferInstance.mask = instance.instanceMask;
            bufferInstance.instanceShaderBindingTableRecordOffset = instance.instanceContributionToHitGroupIndex;
            bufferInstance.flags = instance.cullDisable ? VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR : 0;
            memcpy(bufferInstance.transform.matrix, instance.transform.m, sizeof(bufferInstance.transform.matrix));

            VkBufferDeviceAddressInfo blasAddressInfo = {};
            blasAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            blasAddressInfo.buffer = interfaceBottomLevelAS->vk;
            bufferInstance.accelerationStructureReference = vkGetBufferDeviceAddress(vk, &blasAddressInfo) + instance.bottomLevelAS.offset;
        }

        // Retrieve the size the TLAS will require.
        VkAccelerationStructureGeometryKHR topGeometry = {};
        topGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        topGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;

        VkAccelerationStructureGeometryInstancesDataKHR &instancesData = topGeometry.geometry.instances;
        instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
        buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildGeometryInfo.flags = toRTASBuildFlags(preferFastBuild, preferFastTrace);
        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.pGeometries = &topGeometry;
        buildGeometryInfo.geometryCount = 1;

        VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {};
        buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(vk, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &instanceCount, &buildSizesInfo);

        buildInfo.instanceCount = instanceCount;
        buildInfo.preferFastBuild = preferFastBuild;
        buildInfo.preferFastTrace = preferFastTrace;
        buildInfo.scratchSize = roundUp(buildSizesInfo.buildScratchSize, AccelerationStructureBufferAlignment);
        buildInfo.accelerationStructureSize = roundUp(buildSizesInfo.accelerationStructureSize, AccelerationStructureBufferAlignment);
    }
    
    void VulkanDevice::setShaderBindingTableInfo(RenderShaderBindingTableInfo &tableInfo, const RenderShaderBindingGroups &groups, const RenderPipeline *pipeline, RenderDescriptorSet **descriptorSets, uint32_t descriptorSetCount) {
        assert(pipeline != nullptr);
        assert((descriptorSets != nullptr) && "Vulkan doesn't require descriptor sets, but they should be passed to keep consistency with D3D12.");

        const VulkanRaytracingPipeline *raytracingPipeline = static_cast<const VulkanRaytracingPipeline *>(pipeline);
        assert((raytracingPipeline->type == VulkanPipeline::Type::Raytracing) && "Only raytracing pipelines can be used to build shader binding tables.");
        assert((raytracingPipeline->descriptorSetCount <= descriptorSetCount) && "There must be enough descriptor sets available for the pipeline.");

        const uint32_t handleSize = rtPipelineProperties.shaderGroupHandleSize;
        thread_local std::vector<uint8_t> groupHandles;
        groupHandles.clear();
        groupHandles.resize(raytracingPipeline->groupCount * handleSize, 0);
        VkResult res = vkGetRayTracingShaderGroupHandlesKHR(vk, raytracingPipeline->vk, 0, raytracingPipeline->groupCount, groupHandles.size(), groupHandles.data());
        if (res != VK_SUCCESS) {
            fprintf(stderr, "vkGetRayTracingShaderGroupHandlesKHR failed with error code 0x%X.\n", res);
            return;
        }
        
        const uint32_t handleSizeAligned = roundUp(handleSize, rtPipelineProperties.shaderGroupHandleAlignment);
        const uint32_t regionAlignment = roundUp(handleSizeAligned, rtPipelineProperties.shaderGroupBaseAlignment);
        uint64_t tableSize = 0;

        auto setGroup = [&](RenderShaderBindingGroupInfo &groupInfo, const RenderShaderBindingGroup &renderGroup) {
            groupInfo.startIndex = 0;

            if (renderGroup.pipelineProgramsCount == 0) {
                groupInfo.stride = 0;
                groupInfo.offset = 0;
                groupInfo.size = 0;
            }
            else {
                groupInfo.stride = regionAlignment;
                groupInfo.offset = tableSize;
                groupInfo.size = groupInfo.stride * renderGroup.pipelineProgramsCount;
                tableSize += groupInfo.size;
            }
        };

        setGroup(tableInfo.groups.rayGen, groups.rayGen);
        setGroup(tableInfo.groups.miss, groups.miss);
        setGroup(tableInfo.groups.hitGroup, groups.hitGroup);
        setGroup(tableInfo.groups.callable, groups.callable);

        tableSize = roundUp(tableSize, ShaderBindingTableAlignment);
        tableInfo.tableBufferData.clear();
        tableInfo.tableBufferData.resize(tableSize, 0);

        auto copyGroupData = [&](RenderShaderBindingGroupInfo &groupInfo, const RenderShaderBindingGroup &renderGroup) {
            for (uint32_t i = 0; i < renderGroup.pipelineProgramsCount; i++) {
                const uint8_t *shaderId = groupHandles.data() + renderGroup.pipelinePrograms[i].programIndex * handleSize;
                const uint64_t tableOffset = groupInfo.offset + i * groupInfo.stride;
                memcpy(&tableInfo.tableBufferData[tableOffset], shaderId, handleSize);
            }
        };

        copyGroupData(tableInfo.groups.rayGen, groups.rayGen);
        copyGroupData(tableInfo.groups.miss, groups.miss);
        copyGroupData(tableInfo.groups.hitGroup, groups.hitGroup);
        copyGroupData(tableInfo.groups.callable, groups.callable);
    }

    const RenderDeviceCapabilities &VulkanDevice::getCapabilities() const {
        return capabilities;
    }

    const RenderDeviceDescription &VulkanDevice::getDescription() const {
        return description;
    }

    RenderSampleCounts VulkanDevice::getSampleCountsSupported(RenderFormat format) const {
        if (RenderFormatIsDepth(format)) {
            return RenderSampleCounts(physicalDeviceProperties.limits.framebufferDepthSampleCounts);
        }
        else {
            return RenderSampleCounts(physicalDeviceProperties.limits.framebufferColorSampleCounts);
        }
    }

    void VulkanDevice::release() {
        nullBuffer = nullptr; // force destruction before destroying allocator

        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
        }

        if (vk != VK_NULL_HANDLE) {
            vkDestroyDevice(vk, nullptr);
            vk = VK_NULL_HANDLE;
        }
    }

    bool VulkanDevice::isValid() const {
        return vk != nullptr;
    }

    bool VulkanDevice::beginCapture() {
        assert(false && "Captures are not currently implemented in Vulkan.");
        return false;
    }

    bool VulkanDevice::endCapture() {
        assert(false && "Captures are not currently implemented in Vulkan.");
        return false;
    }

    // VulkanInterface

#if PLUME_SDL_VULKAN_ENABLED
    VulkanInterface::VulkanInterface(RenderWindow sdlWindow) {
#else
    VulkanInterface::VulkanInterface() {
#endif
        VkResult res = volkInitialize();
        if (res != VK_SUCCESS) {
            fprintf(stderr, "volkInitialize failed with error code 0x%X.\n", res);
            return;
        }

        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "plume";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "plume";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.enabledLayerCount = 0;

#   ifdef __APPLE__
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#   endif

        // Check for extensions.
        uint32_t extensionCount;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        std::unordered_set<std::string> requiredExtensions = RequiredInstanceExtensions;
        std::unordered_set<std::string> supportedOptionalExtensions;
#   if DLSS_ENABLED
        const std::unordered_set<std::string> dlssExtensions = DLSS::getRequiredInstanceExtensionsVulkan();
#   endif

#   if PLUME_SDL_VULKAN_ENABLED && !defined(__ANDROID__)
        // Push the extensions specified by SDL as required.
        // SDL2 has this awkward requirement for the window to pull the extensions from. 
        // This can be removed when upgrading to SDL3.
        if (sdlWindow != nullptr) {
            uint32_t sdlVulkanExtensionCount = 0;
            if (SDL_Vulkan_GetInstanceExtensions(sdlWindow, &sdlVulkanExtensionCount, nullptr)) {
                std::vector<char *> sdlVulkanExtensions;
                sdlVulkanExtensions.resize(sdlVulkanExtensionCount);
                if (SDL_Vulkan_GetInstanceExtensions(sdlWindow, &sdlVulkanExtensionCount, (const char **)(sdlVulkanExtensions.data()))) {
                    for (char *sdlVulkanExtension : sdlVulkanExtensions) {
                        requiredExtensions.insert(sdlVulkanExtension);
                    }
                }
            }
        }
#   endif

        std::unordered_set<std::string> missingRequiredExtensions = requiredExtensions;
        for (uint32_t i = 0; i < extensionCount; i++) {
            const std::string extensionName(availableExtensions[i].extensionName);
            missingRequiredExtensions.erase(extensionName);

            if (OptionalInstanceExtensions.find(extensionName) != OptionalInstanceExtensions.end()) {
                supportedOptionalExtensions.insert(extensionName);
            }
#       if DLSS_ENABLED
            else if (dlssExtensions.find(extensionName) != dlssExtensions.end()) {
                supportedOptionalExtensions.insert(extensionName);
            }
#       endif
        }

        if (!missingRequiredExtensions.empty()) {
            for (const std::string &extension : missingRequiredExtensions) {
                fprintf(stderr, "Missing required extension: %s.\n", extension.c_str());
            }

            fprintf(stderr, "Unable to create instance. Required extensions are missing.\n");
            return;
        }

        std::vector<const char *> enabledExtensions;
        for (const std::string &extension : requiredExtensions) {
            enabledExtensions.push_back(extension.c_str());
        }

        for (const std::string &extension : supportedOptionalExtensions) {
            enabledExtensions.push_back(extension.c_str());
        }

        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
        createInfo.enabledExtensionCount = uint32_t(enabledExtensions.size());

#   ifdef VULKAN_VALIDATION_LAYER_ENABLED
        // Search for validation layer and enabled it.
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        
        const char validationLayerName[] = "VK_LAYER_KHRONOS_validation";
        const char *enabledLayerNames[] = { validationLayerName };
        for (const VkLayerProperties &layerProperties : availableLayers) {
            if (strcmp(layerProperties.layerName, validationLayerName) == 0) {
                createInfo.ppEnabledLayerNames = enabledLayerNames;
                createInfo.enabledLayerCount = 1;
                break;
            }
        }
#   endif
        
        tooie_android_debug_log("Plume: vkCreateInstance begin");
        res = vkCreateInstance(&createInfo, nullptr, &instance);
        if (res != VK_SUCCESS) {
            tooie_android_debug_log("Plume: vkCreateInstance failed");
            fprintf(stderr, "vkCreateInstance failed with error code 0x%X.\n", res);
            return;
        }

        tooie_android_debug_log("Plume: vkCreateInstance succeeded");
        volkLoadInstance(instance);

        // Fill capabilities.
        capabilities.shaderFormat = RenderShaderFormat::SPIRV;

        // Fill device names.
        tooie_android_debug_log("Plume: enumerating physical devices");
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount > 0) {
            std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

            for (uint32_t i = 0; i < deviceCount; i++) {
                VkPhysicalDeviceProperties deviceProperties;
                vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
                uint32_t deviceTypeIndex = deviceProperties.deviceType;
                if (deviceTypeIndex <= 4) {
                    deviceNames.emplace_back(deviceProperties.deviceName);
                }
            }
        }
    }

    VulkanInterface::~VulkanInterface() {
        if (instance != nullptr) {
            vkDestroyInstance(instance, nullptr);
        }
    }

    std::unique_ptr<RenderDevice> VulkanInterface::createDevice(const std::string &preferredDeviceName) {
        std::unique_ptr<VulkanDevice> createdDevice = std::make_unique<VulkanDevice>(this, preferredDeviceName);
        return createdDevice->isValid() ? std::move(createdDevice) : nullptr;
    }

    const RenderInterfaceCapabilities &VulkanInterface::getCapabilities() const {
        return capabilities;
    }

    const std::vector<std::string> &VulkanInterface::getDeviceNames() const {
        return deviceNames;
    }

    bool VulkanInterface::isValid() const {
        return instance != nullptr;
    }

    // Global creation function.

#if PLUME_SDL_VULKAN_ENABLED
    std::unique_ptr<RenderInterface> CreateVulkanInterface(RenderWindow sdlWindow) {
        std::unique_ptr<VulkanInterface> createdInterface = std::make_unique<VulkanInterface>(sdlWindow);
        return createdInterface->isValid() ? std::move(createdInterface) : nullptr;
    }
#else
    std::unique_ptr<RenderInterface> CreateVulkanInterface() {
        std::unique_ptr<VulkanInterface> createdInterface = std::make_unique<VulkanInterface>();
        return createdInterface->isValid() ? std::move(createdInterface) : nullptr;
    }
#endif
};
