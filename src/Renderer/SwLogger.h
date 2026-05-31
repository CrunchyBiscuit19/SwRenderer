#pragma once

#include <quill/Logger.h>

#include <cstdint>
#include <unordered_set>
#include <vulkan/vulkan.hpp>

#if defined(_MSC_VER)
    #define SW_DEBUG_BREAK() __debugbreak()
#elif defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
    #define SW_DEBUG_BREAK() __builtin_debugtrap()
#else
    #include <csignal>
    #define SW_DEBUG_BREAK() raise(SIGTRAP)
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

class SwLogger {
public:
    enum class LogLocation { Console, File, Both };
    static constexpr LogLocation LOG_LOCATION{LogLocation::Both};
    static constexpr quill::LogLevel LOG_LEVEL{quill::LogLevel::Debug};

    SwLogger();

    inline quill::Logger* getLogger() { return mLogger; }
    inline std::uint64_t getFrameNumber() const { return mFrameNumber ? *mFrameNumber : 0; }
    inline void setFrameNumber(const std::uint64_t* ptr) { mFrameNumber = ptr; }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
    );

private:
    quill::Logger* mLogger{nullptr};
    const std::uint64_t* mFrameNumber{nullptr};
    std::unordered_set<std::string> mBlockedMessages;
    std::unordered_set<std::string> mBreakMessages;
};
