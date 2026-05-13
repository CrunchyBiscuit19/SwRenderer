#pragma once

#include <Resources/SwBuffer.h>
#include <Resources/SwImage.h>
#include <Resources/SwDescriptor.h>
#include <Resources/SwIResizable.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <vulkan/vulkan_raii.hpp>

class SwFrame {
private:
    vk::raii::CommandPool mCommandPool;
    vk::raii::CommandBuffer mCommandBuffer;
    vk::raii::Fence mRenderFence;
    vk::raii::Semaphore mAvailableSemaphore;
    SwAllocatedBuffer mPerspectiveBuffer;

public:
    SwFrame();

    static void init();

    void initialize();
};

class SwSwapchain {
private:
    vk::raii::SwapchainKHR mSwapchain;
    std::vector<SwSwapchainImage> mImages;
    std::vector<SwFrame> mFrames;
    std::uint64_t mFrameNumber{0};
    std::optional<std::uint64_t> mProgramEndFrameNumber{std::nullopt};
    bool mResizeRequested{false};
    std::uint32_t mSwapchainIndex{0};
    SwColorImage2D mDrawImage;
    SwDepthImage2D mDepthImage;

    void addResizeObserver(SwIResizable& resizable);
    void updateResizeObservers();

public:
    SwSwapchain();

    static void init();

    void initialize();

    SwFrame& getCurrentFrame();
    SwFrame& getPreviousFrame();
    SwSwapchainImage& getCurrentSwapchainImage();
    vk::ResultValue<std::uint32_t> acquireNextImage(uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence);
};