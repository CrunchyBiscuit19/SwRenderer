#pragma once

#include <Data/SwCamera.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwCommandBuffer.h>
#include <Resource/SwCommandPool.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwFence.h>
#include <Resource/SwIResizable.h>
#include <Resource/SwImage.h>
#include <Resource/SwSemaphore.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwFrame {
private:
    static constexpr std::uint32_t PER_FRAME_BUFFER_SIZE{sizeof(SwPerspective)};


    SwCommandPool mCommandPool;
    SwCommandBuffer mCommandBuffer;
    SwFence mRenderFence;
    SwSemaphore mAvailableSemaphore;
    SwAllocatedBuffer mPerFrameBuffer;

public:
    SwFrame();

    static void init();

    void initialize();

    void update();

    inline SwCommandBuffer& getCommandBuffer() { return mCommandBuffer; };
    inline SwFence& getRenderFence() { return mRenderFence; };
    inline SwSemaphore& getAvailableSemaphore() { return mAvailableSemaphore; };
    inline SwAllocatedBuffer& getPerFrameBuffer() { return mPerFrameBuffer; };
};

class SwSwapchain {
private:

    SDL_Window* mWindow{nullptr};
    vk::raii::SurfaceKHR mSurface;

    vk::raii::SwapchainKHR mSwapchain;
    std::vector<SwSwapchainImage> mSwapchainImages;
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

public:
    static vk::ClearColorValue DRAW_CLEAR_VALUE;
    static constexpr vk::Format SRGB_FORMAT{vk::Format::eB8G8R8A8Srgb};
    static constexpr vk::Format UNORM_FORMAT{vk::Format::eB8G8R8A8Unorm};
    static constexpr vk::Format DRAW_FORMAT{vk::Format::eR16G16B16A16Sfloat};
    static constexpr vk::Format DEPTH_FORMAT{vk::Format::eD32Sfloat};
    static constexpr std::uint32_t NUM_SWAPCHAIN_IMAGES{3};
    static constexpr std::uint32_t NUM_FRAME_OVERLAP{2};
    static constexpr std::uint32_t WINDOW_WIDTH_STARTUP{1700};
    static constexpr std::uint32_t WINDOW_HEIGHT_STARTUP{900};
    static constexpr std::uint32_t NUM_FULLSCREEN_QUAD_VERTICES{3};

    SwSwapchain();

    static void init();

    void initialize(SDL_Window* window, vk::raii::SurfaceKHR surface, vk::Extent2D windowExtent, bool windowFullScreen);
    void onResizeInitialize();

    void resize();

    inline SwColorImage2D& getDrawImage() { return mDrawImage; }
    inline SwDepthImage2D& getDepthImage() { return mDepthImage; }
    inline std::uint64_t getFrameNumber() const { return mFrameNumber; }
    inline const std::uint64_t* getFrameNumberPtr() const { return &mFrameNumber; }
    inline void incrementFrameNumber() { mFrameNumber++; }
    inline std::optional<std::uint64_t> getProgramEndFrameNumber() const { return mProgramEndFrameNumber; }
    inline void setProgramEndFrameNumber(std::uint64_t programEndFrameNumber) { mProgramEndFrameNumber = std::optional<std::uint64_t>(programEndFrameNumber); }
    inline SDL_Window* getWindowPtr() const { return mWindow; }
    inline float getAspectRatio() const { return mAspectRatio; }
    inline SwFrame& getCurrentFrame() { return mFrames[mFrameNumber % NUM_FRAME_OVERLAP]; }
    inline SwFrame& getPreviousFrame() { return mFrames[(mFrameNumber - 1) % NUM_FRAME_OVERLAP]; }
    inline bool getResizeRequested() const { return mResizeRequested; }
    inline void setResizeRequested(bool resizeRequested) { mResizeRequested = resizeRequested; }
    inline vk::Extent2D getWindowExtent2D() const { return mWindowExtent; }
    inline vk::Extent3D getWindowExtent3D() const { return vk::Extent3D{mWindowExtent, 1}; }
    inline SwSwapchainImage& getCurrentSwapchainImage() { return mSwapchainImages[mSwapchainIndex]; }

    void acquireNextImage(uint64_t timeout);

    void submit(
        vk::ArrayProxy<vk::CommandBufferSubmitInfo> commandBufferSubmitInfo, vk::ArrayProxy<vk::SemaphoreSubmitInfo> waitSemaphoreInfo,
        vk::ArrayProxy<vk::SemaphoreSubmitInfo> signalSemaphoreInfo, vk::Fence renderFence
    );

    void present();

    ~SwSwapchain();
};