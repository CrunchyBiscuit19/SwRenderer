#pragma once

#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwStats.h>
#include <Renderer/SwSwapchain.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_events.h>
#include <quill/Logger.h>
#include <vk_mem_alloc.h>

#include <functional>
#include <vulkan/vulkan.hpp>

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

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
);

struct SwRendererContext {
    vk::raii::Device* mDevice;
    VmaAllocator mAllocator;
    SwImmSubmit* mImmSubmit;

    SwRendererContext() = default;

    SwRendererContext(vk::raii::Device* device, VmaAllocator allocator, SwImmSubmit* immSubmit)
        : mDevice(device), mAllocator(allocator), mImmSubmit(immSubmit) {};
};

struct SwImmSubmitContext {
    vk::raii::Device* mDevice;
    VmaAllocator mAllocator;
    vk::raii::Queue* mGraphicsQueue;

    SwImmSubmitContext() = default;

    SwImmSubmitContext(vk::raii::Device* device, VmaAllocator allocator, vk::raii::Queue* graphicsQueue)
        : mDevice(device), mAllocator(allocator), mGraphicsQueue(graphicsQueue) {};
};

struct SwSwapchainContext {
    vk::raii::Device* mDevice;
    vk::raii::PhysicalDevice* mChosenGPU;
    vk::raii::SurfaceKHR* mSurface;
    vk::Extent2D mWindowExtent;

    SwSwapchainContext() = default;

    SwSwapchainContext(vk::raii::Device* device, vk::raii::PhysicalDevice* chosenGPU, vk::raii::SurfaceKHR* surface, vk::Extent2D windowExtent)
        : mDevice(device), mChosenGPU(chosenGPU), mSurface(surface), mWindowExtent(windowExtent) {};
};

struct SwVmaAllocator {
    VmaAllocator mAllocator;

    SwVmaAllocator() = default;

    SwVmaAllocator(VmaAllocator allocator) : mAllocator(allocator) {};

    ~SwVmaAllocator() {
        if (mAllocator == nullptr) return;
        vmaDestroyAllocator(mAllocator);
        mAllocator = nullptr;
    }
};

class SwRenderer {
    enum class ValidationMode { None, Basic, Strict };
    enum class LogLocation { Console, File, Both };

    static const ValidationMode VALIDATION_MODE{ValidationMode::Basic};
    static const LogLocation LOG_LOCATION{LogLocation::Both};
    static const std::uint32_t VK_MAJOR_VERSION{1};
    static const std::uint32_t VK_MINOR_VERSION{4};
    static const std::uint32_t VK_PATCH_VERSION{0};

    vk::raii::Context mContext;
    vk::raii::Instance mInstance;
    vk::raii::PhysicalDevice mChosenGPU;
    vk::raii::Device mDevice;
    vk::PhysicalDeviceProperties mChosenGPUProperties;
    vk::raii::SurfaceKHR mSurface;
    vk::raii::DebugUtilsMessengerEXT mDebugMessenger;
    vk::raii::Queue mComputeQueue;
    std::uint32_t mComputeQueueFamily;
    vk::raii::Queue mGraphicsQueue;
    std::uint32_t mGraphicsQueueFamily;
    SwVmaAllocator mAllocator;
    SDL_Window* mWindow{nullptr};
    vk::Extent2D mWindowExtent{1700, 900};
    float mAspectRatio{static_cast<float>(mWindowExtent.width) / static_cast<float>(mWindowExtent.height)};
    bool mWindowFullScreen{false};
    bool mIsInitialized{false};
    bool mStopRendering{false};

    quill::Logger* mLogger;
    std::vector<std::function<void(SDL_Event& e)>> mEventCallbacks;

    SwImmSubmit mImmSubmit;
    SwSwapchain mSwapchain;
    SwStats mStats;

    SwRendererContext mRendererContext;
    SwImmSubmitContext mImmSubmitContext;
    SwSwapchainContext mSwapchainContext;

public:
    SwRenderer();

    inline quill::Logger* getLogger() { return mLogger; };

    inline SwRendererContext getRendererInfo() { return mRendererContext; };

    void addEventCallback(const std::function<void(SDL_Event& e)>& inputCallback);

    void executeEventCallbacks(SDL_Event& e) const;

    template <typename T>
    inline void labelResourceDebug(T& resource, const char* name) {
        vk::DebugUtilsObjectNameInfoEXT nameInfo{VulkanResourceInfo<T>::resourceType, VulkanResourceInfo<T>::getHandle(resource), name};
        mDevice.setDebugUtilsObjectNameEXT(nameInfo);
    };

    void cleanup();
};