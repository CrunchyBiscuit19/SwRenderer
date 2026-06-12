#pragma once

#include <Renderer/SwStats.h>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

// See SW_ENABLE_DEBUG_LABELS in CMakeLists. Default to not using.
#ifndef SW_ENABLE_DEBUG_LABELS
#define SW_ENABLE_DEBUG_LABELS 0
#endif

template <typename T>
struct VulkanResourceInfo;

#define DEFINE_VULKAN_RESOURCE_INFO(HppType, VkType, resourceTypeEnum)                                                                     \
    template <>                                                                                                                            \
    struct VulkanResourceInfo<HppType> {                                                                                                   \
        static constexpr vk::ObjectType resourceType = resourceTypeEnum;                                                                   \
        static std::uint64_t getHandle(const HppType& resource) { return reinterpret_cast<std::uint64_t>(static_cast<VkType>(resource)); } \
    };

#define DEFINE_VULKAN_RAII_RESOURCE_INFO(RaiiType, VkType, resourceTypeEnum)                                                                 \
    template <>                                                                                                                              \
    struct VulkanResourceInfo<RaiiType> {                                                                                                    \
        static constexpr vk::ObjectType resourceType = resourceTypeEnum;                                                                     \
        static std::uint64_t getHandle(const RaiiType& resource) { return reinterpret_cast<std::uint64_t>(static_cast<VkType>(*resource)); } \
    };

DEFINE_VULKAN_RESOURCE_INFO(vk::Buffer, VkBuffer, vk::ObjectType::eBuffer)
DEFINE_VULKAN_RESOURCE_INFO(vk::Image, VkImage, vk::ObjectType::eImage)
DEFINE_VULKAN_RESOURCE_INFO(vk::ImageView, VkImageView, vk::ObjectType::eImageView)
DEFINE_VULKAN_RESOURCE_INFO(vk::ShaderModule, VkShaderModule, vk::ObjectType::eShaderModule)
DEFINE_VULKAN_RESOURCE_INFO(vk::Pipeline, VkPipeline, vk::ObjectType::ePipeline)
DEFINE_VULKAN_RESOURCE_INFO(vk::PipelineLayout, VkPipelineLayout, vk::ObjectType::ePipelineLayout)
DEFINE_VULKAN_RESOURCE_INFO(vk::DescriptorSetLayout, VkDescriptorSetLayout, vk::ObjectType::eDescriptorSetLayout)
DEFINE_VULKAN_RESOURCE_INFO(vk::DescriptorSet, VkDescriptorSet, vk::ObjectType::eDescriptorSet)
DEFINE_VULKAN_RESOURCE_INFO(vk::CommandPool, VkCommandPool, vk::ObjectType::eCommandPool)
DEFINE_VULKAN_RESOURCE_INFO(vk::CommandBuffer, VkCommandBuffer, vk::ObjectType::eCommandBuffer)
DEFINE_VULKAN_RESOURCE_INFO(vk::Fence, VkFence, vk::ObjectType::eFence)
DEFINE_VULKAN_RESOURCE_INFO(vk::Semaphore, VkSemaphore, vk::ObjectType::eSemaphore)
DEFINE_VULKAN_RESOURCE_INFO(vk::Sampler, VkSampler, vk::ObjectType::eSampler)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Buffer, VkBuffer, vk::ObjectType::eBuffer)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Image, VkImage, vk::ObjectType::eImage)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::ImageView, VkImageView, vk::ObjectType::eImageView)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::ShaderModule, VkShaderModule, vk::ObjectType::eShaderModule)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Pipeline, VkPipeline, vk::ObjectType::ePipeline)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::PipelineLayout, VkPipelineLayout, vk::ObjectType::ePipelineLayout)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::DescriptorSetLayout, VkDescriptorSetLayout, vk::ObjectType::eDescriptorSetLayout)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::DescriptorSet, VkDescriptorSet, vk::ObjectType::eDescriptorSet)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::CommandPool, VkCommandPool, vk::ObjectType::eCommandPool)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::CommandBuffer, VkCommandBuffer, vk::ObjectType::eCommandBuffer)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Fence, VkFence, vk::ObjectType::eFence)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Semaphore, VkSemaphore, vk::ObjectType::eSemaphore)
DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Sampler, VkSampler, vk::ObjectType::eSampler)

class SwImmSubmit;
class SwEvents;
class SwCamera;
class SwSwapchain;
class SwDescriptorAllocator;
class SwScene;
class SwLogger;

struct SwRendererContext {
    vk::raii::Instance* mInstance;
    vk::raii::PhysicalDevice* mChosenGPU;
    vk::raii::Device* mDevice;
    VmaAllocator mAllocator;
    vk::raii::Queue* mGraphicsQueue;
    vk::raii::Queue* mComputeQueue;
    SwDescriptorAllocator* mDescriptorAllocator;
    SwSwapchain* mSwapchain;
    SwImmSubmit* mImmSubmit;
    SwEvents* mEvents;
    SwScene* mScene;
    SwStats* mStats;
    SwLogger* mLogger;

    SwRendererContext() = default;
    SwRendererContext(
        vk::raii::Instance* instance, vk::raii::PhysicalDevice* chosenGPU, vk::raii::Device* device, VmaAllocator allocator, vk::raii::Queue* graphicsQueue,
        vk::raii::Queue* computeQueue, SwDescriptorAllocator* descriptorAllocator, SwSwapchain* swapchain, SwImmSubmit* immSubmit, SwEvents* events, SwScene* scene,
        SwStats* stats, SwLogger* logger
    );

    template <typename T>
    inline void labelResourceDebug(const T& resource, const char* name) {
#if SW_ENABLE_DEBUG_LABELS
        vk::DebugUtilsObjectNameInfoEXT nameInfo{VulkanResourceInfo<T>::resourceType, VulkanResourceInfo<T>::getHandle(resource), name};
        mDevice->setDebugUtilsObjectNameEXT(nameInfo);
#else
        // Only here to suppress unused parameter warnings when debug labels are disabled
        (void)resource;
        (void)name;
#endif
    };
};
