
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <VkBootstrap.h>
#include <format>

SwFrame::SwFrame()
    : mGraphicsCommandPool(nullptr), mGraphicsCommandBuffer(nullptr), mTransferCommandPool(nullptr), mTransferCommandBuffer(nullptr), mRenderFence(nullptr),
      mAvailableSemaphore(nullptr), mTransferSemaphore(nullptr) {}

void SwFrame::initialize(std::uint32_t frameIndex) {
    mGraphicsCommandPool = SwCommandPoolFactory::createCommandPool(
        std::format("Frame{}GraphicsCommandPool", frameIndex), vk::CommandPoolCreateFlagBits::eResetCommandBuffer, SwRenderer::sRendererContext.mGraphicsQueueFamily
    );
    mGraphicsCommandBuffer = SwCommandBufferFactory::createCommandBuffer(std::format("Frame{}GraphicsCommandBuffer", frameIndex), mGraphicsCommandPool);
    mTransferCommandPool = SwCommandPoolFactory::createCommandPool(
        std::format("Frame{}TransferCommandPool", frameIndex), vk::CommandPoolCreateFlagBits::eResetCommandBuffer, SwRenderer::sRendererContext.mTransferQueueFamily
    );
    mTransferCommandBuffer = SwCommandBufferFactory::createCommandBuffer(std::format("Frame{}TransferCommandBuffer", frameIndex), mTransferCommandPool);
    mRenderFence = SwFenceFactory::createFence(std::format("Frame{}RenderFence", frameIndex), vk::FenceCreateFlagBits::eSignaled);
    mAvailableSemaphore = SwSemaphoreFactory::createSemaphore(std::format("Frame{}AvailableSemaphore", frameIndex));
    mTransferSemaphore = SwSemaphoreFactory::createSemaphore(std::format("Frame{}TransferSemaphore", frameIndex));
    mDataBuffer = SwBufferFactory::createAllocatedBuffer(
        std::format("Frame{}DataBuffer", frameIndex),
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        DATA_BUFFER_SIZE,
        true
    );
}

void SwFrame::update() {
    SwScene& scene = *SwRenderer::sRendererContext.mScene;
    Data data{
        .mCameraBuffer = scene.getCamera().getCameraBuffer().getDeviceAddress().value(),
    };
    mDataBuffer.copyFromUnchecked(&data, sizeof(Data));
}

vk::ClearColorValue SwSwapchain::DRAW_CLEAR_VALUE{.463f, .616f, .859f, 0.f};

SwSwapchain::SwSwapchain() : mSwapchain(nullptr), mSurface(nullptr) {}

void SwSwapchain::initialize(SDL_Window* window, vk::raii::SurfaceKHR surface, vk::Extent2D windowExtent, bool windowFullScreen) {
    mFrames.reserve(NUM_FRAME_OVERLAP);
    for (size_t i = 0; i < NUM_FRAME_OVERLAP; i++) {
        mFrames.emplace_back();
        mFrames.back().initialize(static_cast<std::uint32_t>(i));
    }

    mWindow = window;
    mSurface = std::move(surface);
    mWindowExtent = windowExtent;
    mWindowFullScreen = windowFullScreen;
    mAspectRatio = static_cast<float>(mWindowExtent.width) / static_cast<float>(mWindowExtent.height);
    mWindowFullScreen ? mResizeRequested = true : mResizeRequested = false;  // Initial resize for fullscreen

    onResizeInitialize();
}

void SwSwapchain::toggleFullscreen() {
    mWindowFullScreen = !mWindowFullScreen;
    SDL_SetWindowFullscreen(mWindow, mWindowFullScreen);
    SDL_SetWindowBordered(mWindow, !mWindowFullScreen);
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
            std::format("SwapchainImage{}", i),
            vkbSwapchain.get_images().value()[i],
            formats[0],
            vk::Extent3D(vkbSwapchain.extent, 1),
            SwRenderer::sRendererContext.mDevice->createImageView(srgbImageViewCreateInfo),
            SwSemaphoreFactory::createSemaphore(std::format("SwapchainImage{}RenderedSemaphore", i)),
            {formats[1]},
            std::move(otherImageViews)
        );
        mSwapchainImages.emplace_back(std::move(swapchainImage));
    }

    mDrawImage = SwImageFactory::createColorImage2D(
        "SwapchainDrawImage",
        DRAW_FORMAT,
        vk::Extent3D{mWindowExtent, 1},
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
        false,
        DRAW_CLEAR_VALUE
    );
    mDepthImage = SwImageFactory::createDepthImage2D(
        "SwapchainDepthImage",
        DEPTH_FORMAT,
        mDrawImage.getExtent(),
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eStorage,
        false
    );
    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < mSwapchainImages.size(); i++) {
            mSwapchainImages[i].emitTransition(cmd, SwDependency::ImageDepType::PresentSrc);
        }
        mDrawImage.emitTransition(cmd, SwDependency::ImageDepType::TransferSrc);
        mDepthImage.emitTransition(cmd, SwDependency::ImageDepType::DepthAttachmentReadWrite);
    });
}

void SwSwapchain::resize() { onResizeInitialize(); }

void SwSwapchain::acquireNextImage(uint64_t timeout) {
    try {
        mSwapchainIndex = mSwapchain.acquireNextImage(timeout, getCurrentFrame().getAvailableSemaphore().getHandle(), nullptr).value;
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
    vk::Semaphore renderSemaphore = getCurrentSwapchainImage().getRenderedSemaphore().getHandle();
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
