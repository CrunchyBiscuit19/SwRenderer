#pragma once

#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwLogger.h>
#include <Renderer/SwRendererContext.h>
#include <Scene/SwScene.h>
#include <Renderer/SwStats.h>
#include <Renderer/SwSwapchain.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_events.h>
#include <vk_mem_alloc.h>

#include <functional>
#include <vulkan/vulkan.hpp>

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

    static constexpr std::uint32_t VK_MAJOR_VERSION{1};
    static constexpr std::uint32_t VK_MINOR_VERSION{4};
    static constexpr std::uint32_t VK_PATCH_VERSION{0};
    static constexpr bool FULLSCREEN_ON_STARTUP{false};

public:
    static SwRendererContext sRendererContext;

private:
    SwLogger mLogger;
    vk::raii::Context mContext;
    vk::raii::Instance mInstance;
    vk::raii::PhysicalDevice mChosenGPU;
    vk::raii::Device mDevice;
    vk::PhysicalDeviceProperties mChosenGPUProperties;
    vk::raii::DebugUtilsMessengerEXT mDebugMessenger;
    vk::raii::Queue mComputeQueue;
    std::uint32_t mComputeQueueFamily;
    vk::raii::Queue mGraphicsQueue;
    std::uint32_t mGraphicsQueueFamily;
    SwVmaAllocator mAllocator;
    SwDescriptorAllocator mDescriptorAllocator;
    bool mIsInitialized{false};
    bool mStopRendering{false};

    SwImmSubmit mImmSubmit;
    SwSwapchain mSwapchain;
    SwStats mStats;
    SwEvents mEvents;

    SwScene mScene;

public:
    static constexpr ValidationMode VALIDATION_MODE{ValidationMode::Basic};

    static constexpr std::uint32_t ONE_SECOND_IN_MS{1000};
    static constexpr std::uint32_t EXPECTED_FRAME_RATE{60};
    static constexpr std::uint32_t MAX_1D_WORKGROUP_THREADS{1 << 10};
    static constexpr std::uint32_t MAX_2D_WORKGROUP_THREADS{1 << 5};
    static constexpr std::uint32_t MAX_DESCRIPTOR_SETS{1 << 12};

    SwRenderer();

    inline SwRendererContext& getRendererInfo() { return sRendererContext; };

    inline std::uint64_t getFrameNumber() const { return mSwapchain.getFrameNumber(); };

    template <typename T>
    inline void labelResourceDebug(T& resource, const char* name) {
        vk::DebugUtilsObjectNameInfoEXT nameInfo{VulkanResourceInfo<T>::resourceType, VulkanResourceInfo<T>::getHandle(resource), name};
        mDevice.setDebugUtilsObjectNameEXT(nameInfo);
    };

    void run();
    void draw();

    ~SwRenderer();
};