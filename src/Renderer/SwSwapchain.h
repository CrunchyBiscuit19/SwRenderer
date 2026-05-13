#pragma once

#include <Resources/SwBuffer.h>
#include <Resources/SwCommandBuffer.h>
#include <Resources/SwCommandPool.h>
#include <Resources/SwDescriptor.h>
#include <Resources/SwIResizable.h>
#include <Resources/SwImage.h>
#include <Resources/SwSemaphore.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <vulkan/vulkan_raii.hpp>

struct SwSwapchainContext;

class SwFrame {
private:
    SwCommandPool mCommandPool;
    SwCommandBuffer mCommandBuffer;
    SwFence mRenderFence;
    SwSemaphore mAvailableSemaphore;
    SwAllocatedBuffer mPerspectiveBuffer;

public:
    SwFrame();

    void initialize();
};

class SwSwapchain {
private:
    static const std::uint32_t NUM_SWAPCHAIN_IMAGES{3};
    static const std::uint32_t NUM_FRAME_OVERLAP{2};
    static const std::uint32_t SRGB_INDEX{0};
    static const std::uint32_t UNORM_INDEX{1};

    static SwSwapchainContext sSwapchainContext;

    SDL_Window* mWindow{nullptr};
    vk::raii::SurfaceKHR mSurface;

    vk::raii::SwapchainKHR mSwapchain;
    std::vector<SwSwapchainImage> mImages;
    std::uint32_t mSwapchainIndex{0};
    bool mResizeRequested{false};

    std::vector<SwFrame> mFrames;
    std::uint64_t mFrameNumber{0};
    std::optional<std::uint64_t> mProgramEndFrameNumber{std::nullopt};

    vk::Extent2D mWindowExtent{1700, 900};
    float mAspectRatio{static_cast<float>(mWindowExtent.width) / static_cast<float>(mWindowExtent.height)};
    bool mWindowFullScreen{false};

    SwColorImage2D mDrawImage;
    SwDepthImage2D mDepthImage;

    void addResizeObserver(SwIResizable& resizable);
    void updateResizeObservers();

public:
    SwSwapchain();

    static void init(SwSwapchainContext swapchainContext);

    void initialize(SDL_Window* window, vk::raii::SurfaceKHR surface, vk::Extent2D windowExtent, bool windowFullScreen);

    SwFrame& getCurrentFrame();

    SwFrame& getPreviousFrame();

    SwSwapchainImage& getCurrentSwapchainImage();

    vk::ResultValue<std::uint32_t> acquireNextImage(uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence);

    ~SwSwapchain();
};