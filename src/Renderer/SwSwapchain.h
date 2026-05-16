#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwCommandBuffer.h>
#include <Resource/SwCommandPool.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwIResizable.h>
#include <Resource/SwImage.h>
#include <Resource/SwSemaphore.h>
#include <Resource/SwFence.h>
#include <Data/SwCamera.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <vulkan/vulkan_raii.hpp>

struct SwSwapchainContext;

class SwFrame {
private:
    static const std::uint32_t PER_FRAME_BUFFER_SIZE{sizeof(SwPerspective)};

    SwCommandPool mCommandPool;
    SwCommandBuffer mCommandBuffer;
    SwFence mRenderFence;
    SwSemaphore mAvailableSemaphore;
    SwAllocatedBuffer mPerFrameBuffer;

public:
    SwFrame();

    void initialize();

    inline SwSemaphore& getAvailableSemaphore() { return mAvailableSemaphore; };
};

class SwSwapchain {
private:
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

    vk::Extent2D mWindowExtent{WINDOW_WIDTH_STARTUP, WINDOW_HEIGHT_STARTUP};
    float mAspectRatio{static_cast<float>(mWindowExtent.width) / static_cast<float>(mWindowExtent.height)};
    bool mWindowFullScreen{false};

    SwColorImage2D mDrawImage;
    SwDepthImage2D mDepthImage;

    void addResizeObserver(SwIResizable& resizable);
    void updateResizeObservers();

public:
    static const vk::Format SRGB_FORMAT{vk::Format::eB8G8R8A8Srgb};
    static const vk::Format UNORM_FORMAT{vk::Format::eB8G8R8A8Unorm};
    static const vk::Format DRAW_FORMAT{vk::Format::eR16G16B16A16Sfloat};
    static const vk::Format DEPTH_FORMAT{vk::Format::eD32Sfloat};
    static const std::uint32_t NUM_SWAPCHAIN_IMAGES{3};
    static const std::uint32_t NUM_FRAME_OVERLAP{2};
    static const std::uint32_t WINDOW_WIDTH_STARTUP{1700};
    static const std::uint32_t WINDOW_HEIGHT_STARTUP{900};

    SwSwapchain();

    static void init(SwSwapchainContext swapchainContext);

    void initialize(SDL_Window* window, vk::raii::SurfaceKHR surface, vk::Extent2D windowExtent, bool windowFullScreen);

    inline std::uint32_t getFrameNumber() const { return mFrameNumber; }
    inline SDL_Window* getWindow() const { return mWindow; }
    inline float getAspectRatio() const { return mAspectRatio; }

    SwFrame& getCurrentFrame();
    SwFrame& getPreviousFrame();

    SwSwapchainImage& getCurrentSwapchainImage();

    void acquireNextImage(uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence);

    ~SwSwapchain();
};