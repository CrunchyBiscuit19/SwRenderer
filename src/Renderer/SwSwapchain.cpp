
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <VkBootstrap.h>
#include <fmt/core.h>

SwFrame::SwFrame() : mCommandPool(nullptr), mCommandBuffer(nullptr), mRenderFence(nullptr), mAvailableSemaphore(nullptr) {}

void SwFrame::initialize(std::uint32_t frameIndex) {
    mCommandPool = SwCommandPoolFactory::createCommandPool(fmt::format("Frame{}CommandPool", frameIndex), vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    mCommandBuffer = SwCommandBufferFactory::createCommandBuffer(fmt::format("Frame{}CommandBuffer", frameIndex), mCommandPool);
    mRenderFence = SwFenceFactory::createFence(fmt::format("Frame{}RenderFence", frameIndex), vk::FenceCreateFlagBits::eSignaled);
    mAvailableSemaphore = SwSemaphoreFactory::createSemaphore(fmt::format("Frame{}AvailableSemaphore", frameIndex));
    mPerFrameBuffer = SwBufferFactory::createAllocatedBuffer(
        fmt::format("Frame{}PerFrameBuffer", frameIndex), vk::BufferUsageFlagBits::eUniformBuffer, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        PER_FRAME_BUFFER_SIZE, true
    );
}

void SwFrame::update() {
    SwScene& scene = *SwRenderer::sRendererContext.mScene;
    Data perFrameData{
        .mPerspective = scene.getCamera().getPerspective(),
        .mSunlight = scene.getLightingSystem().getSunlight(),
        .mCameraWorldPos = scene.getCamera().getPosition(),
    };
    mPerFrameBuffer.copyFromUnchecked(&perFrameData, sizeof(Data));
}

vk::ClearColorValue SwSwapchain::DRAW_CLEAR_VALUE{.463f, .616f, .859f, 0.f};

SwSwapchain::SwSwapchain() : mSwapchain(nullptr), mSurface(nullptr) {}


void SwSwapchain::initialize(SDL_Window* window, vk::raii::SurfaceKHR surface, vk::Extent2D windowExtent, bool windowFullScreen) {
    mFrames.reserve(NUM_FRAME_OVERLAP);
    for (size_t i = 0; i < NUM_FRAME_OVERLAP; i++) {
        mFrames.emplace_back();
        mFrames.back().initialize(static_cast<std::uint32_t>(i));
    }

    SwRenderer::sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
        const SDL_Keymod modState = SDL_GetModState();
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if ((modState & KMOD_ALT) && keyState[SDL_SCANCODE_RETURN] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mWindowFullScreen = !mWindowFullScreen;
            SDL_SetWindowFullscreen(mWindow, mWindowFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            SDL_SetWindowBordered(mWindow, mWindowFullScreen ? SDL_FALSE : SDL_TRUE);
        }
    });

    mWindow = window;
    mSurface = std::move(surface);
    mWindowExtent = windowExtent;
    mWindowFullScreen = windowFullScreen;
    mAspectRatio = static_cast<float>(mWindowExtent.width) / static_cast<float>(mWindowExtent.height);
    mWindowFullScreen ? mResizeRequested = true : mResizeRequested = false;  // Initial resize for fullscreen

    onResizeInitialize();
}

void SwSwapchain::onResizeInitialize() {
    std::int32_t w, h;
    SDL_GetWindowSize(mWindow, &w, &h);
    mWindowExtent.width = w;
    mWindowExtent.height = h;
    mAspectRatio = static_cast<float>(w) / static_cast<float>(h);

    vk::ImageFormatListCreateInfo formatListCreateInfo{};
    std::vector<vk::Format> formats = {SRGB_FORMAT, UNORM_FORMAT};
    formatListCreateInfo.pViewFormats = formats.data();
    formatListCreateInfo.viewFormatCount = formats.size();

    mSwapchain.clear();
    vkb::SwapchainBuilder swapchainBuilder(**SwRenderer::sRendererContext.mChosenGPU, **SwRenderer::sRendererContext.mDevice, *mSurface);
    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            .set_desired_format(
                VkSurfaceFormatKHR{.format = static_cast<VkFormat>(SRGB_FORMAT), .colorSpace = static_cast<VkColorSpaceKHR>(vk::ColorSpaceKHR::eSrgbNonlinear)}
            )
            .set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eMailbox))
            .set_desired_extent(mWindowExtent.width, mWindowExtent.height)
            .add_image_usage_flags(static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eTransferDst))
            .set_desired_min_image_count(NUM_SWAPCHAIN_IMAGES)
            .set_create_flags(static_cast<VkSwapchainCreateFlagBitsKHR>(vk::SwapchainCreateFlagBitsKHR::eMutableFormat))
            .add_pNext(&formatListCreateInfo)
            .build()
            .value();
    mSwapchain = vk::raii::SwapchainKHR(*SwRenderer::sRendererContext.mDevice, vkbSwapchain.swapchain);

    mSwapchainImages.clear();
    mSwapchainImages.reserve(NUM_SWAPCHAIN_IMAGES);
    for (std::uint32_t i = 0; i < vkbSwapchain.get_images().value().size(); i++) {
        vk::ImageViewCreateInfo srgbImageViewCreateInfo = {};
        srgbImageViewCreateInfo.pNext = nullptr;
        srgbImageViewCreateInfo.viewType = vk::ImageViewType::e2D;
        srgbImageViewCreateInfo.image = vkbSwapchain.get_images().value()[i];
        srgbImageViewCreateInfo.format = SRGB_FORMAT;
        srgbImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        srgbImageViewCreateInfo.subresourceRange.levelCount = 1;
        srgbImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        srgbImageViewCreateInfo.subresourceRange.layerCount = 1;
        srgbImageViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vk::ImageViewCreateInfo unormImageViewCreateInfo = srgbImageViewCreateInfo;
        unormImageViewCreateInfo.format = UNORM_FORMAT;

        std::deque<vk::raii::ImageView> otherImageViews;
        otherImageViews.emplace_back(SwRenderer::sRendererContext.mDevice->createImageView(unormImageViewCreateInfo));

        SwSwapchainImage swapchainImage(
            fmt::format("SwapchainImage{}", i),
            vkbSwapchain.get_images().value()[i],
            formats[0],
            vk::Extent3D(vkbSwapchain.extent, 1),
            SwRenderer::sRendererContext.mDevice->createImageView(srgbImageViewCreateInfo),
            SwSemaphoreFactory::createSemaphore(fmt::format("SwapchainImage{}RenderedSemaphore", i)),
            {formats[1]},
            std::move(otherImageViews)
        );
        mSwapchainImages.emplace_back(std::move(swapchainImage));

    }


    mDrawImage = SwImageFactory::createColorImage2D(
        "SwapchainDrawImage",
        nullptr,
        DRAW_FORMAT,
        vk::Extent3D{mWindowExtent, 1},
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
        false,
        DRAW_CLEAR_VALUE
    );
    mDepthImage = SwImageFactory::createDepthImage2D(
        "SwapchainDepthImage",
        nullptr,
        DEPTH_FORMAT,
        mDrawImage.getExtent(),
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eStorage,
        false
    );
    SwRenderer::sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < mSwapchainImages.size(); i++) {
            mSwapchainImages[i].emitTransition(cmd, SwDependency::ImageDepType::PresentSrc);
        }
        mDrawImage.emitTransition(cmd, SwDependency::ImageDepType::TransferSrc);
        mDepthImage.emitTransition(cmd, SwDependency::ImageDepType::DepthAttachmentReadWrite);
    });
}

void SwSwapchain::resize() {
    onResizeInitialize();
}

void SwSwapchain::acquireNextImage(uint64_t timeout) {
    try {
        mSwapchainIndex = mSwapchain.acquireNextImage(timeout, getCurrentFrame().getAvailableSemaphore().getRawSemaphore(), nullptr).value;
    } catch (vk::OutOfDateKHRError& e) {
        mResizeRequested = true;
    }
}

void SwSwapchain::submit(
    vk::ArrayProxy<vk::CommandBufferSubmitInfo> commandBufferSubmitInfo, vk::ArrayProxy<vk::SemaphoreSubmitInfo> waitSemaphoreInfo,
    vk::ArrayProxy<vk::SemaphoreSubmitInfo> signalSemaphoreInfo, vk::Fence renderFence
) {
    vk::SubmitInfo2 submitInfo = {};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = waitSemaphoreInfo.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreInfo.data();
    submitInfo.signalSemaphoreInfoCount = signalSemaphoreInfo.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreInfo.data();
    submitInfo.commandBufferInfoCount = commandBufferSubmitInfo.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfo.data();
    SwRenderer::sRendererContext.mGraphicsQueue->submit2(submitInfo, renderFence);
}

void SwSwapchain::present() {
    vk::Semaphore renderSemaphore = getCurrentSwapchainImage().getRenderedSemaphore().getRawSemaphore();
    vk::PresentInfoKHR presentInfo = {};
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &(*mSwapchain);
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &mSwapchainIndex;

    try {
        auto _ = SwRenderer::sRendererContext.mGraphicsQueue->presentKHR(presentInfo);
    } catch (vk::OutOfDateKHRError e) {
        mResizeRequested = true;
    }
}

SwSwapchain::~SwSwapchain() {
    mSwapchain.clear();
    mSurface.clear();
    SDL_DestroyWindow(mWindow);
}
