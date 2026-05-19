#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Data/SwCamera.h>
#include <VkBootstrap.h>

SwFrame::SwFrame() : mCommandPool(nullptr), mCommandBuffer(nullptr), mRenderFence(nullptr), mAvailableSemaphore(nullptr) {}

void SwFrame::initialize() {
    mCommandPool = SwCommandPoolFactory::createCommandPool(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    mCommandBuffer = SwCommandBufferFactory::createCommandBuffer(mCommandPool);
    mRenderFence = SwFenceFactory::createFence(vk::FenceCreateFlagBits::eSignaled);
    mAvailableSemaphore = SwSemaphoreFactory::createSemaphore();
    mPerFrameBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        PER_FRAME_BUFFER_SIZE
    );
}

SwRendererContext SwSwapchain::sRendererContext{};

SwSwapchain::SwSwapchain() : mSwapchain(nullptr), mSurface(nullptr) {}

void SwSwapchain::init(SwRendererContext swapchainContext) { sRendererContext = swapchainContext; }

void SwSwapchain::initialize(SDL_Window* window, vk::raii::SurfaceKHR surface, vk::Extent2D windowExtent, bool windowFullScreen) {
    mWindow = window;
    mSurface = std::move(surface);
    mWindowExtent = windowExtent;
    mWindowFullScreen = windowFullScreen;
    mAspectRatio = static_cast<float>(mWindowExtent.width) / static_cast<float>(mWindowExtent.height);
    mWindowFullScreen ? mResizeRequested = true : mResizeRequested = false;  // Initial resize for fullscreen

    sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
        const SDL_Keymod modState = SDL_GetModState();
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if ((modState & KMOD_ALT) && keyState[SDL_SCANCODE_RETURN] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mWindowFullScreen = !mWindowFullScreen;
            SDL_SetWindowFullscreen(mWindow, mWindowFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            SDL_SetWindowBordered(mWindow, mWindowFullScreen ? SDL_FALSE : SDL_TRUE);
        }
    });

    vk::ImageFormatListCreateInfo formatListCreateInfo{};
    std::vector<vk::Format> formats = {vk::Format::eB8G8R8A8Srgb, vk::Format::eB8G8R8A8Unorm};
    formatListCreateInfo.pViewFormats = formats.data();
    formatListCreateInfo.viewFormatCount = formats.size();

    vkb::SwapchainBuilder swapchainBuilder(**sRendererContext.mChosenGPU, **sRendererContext.mDevice, *mSurface);
    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            .set_desired_format(
                VkSurfaceFormatKHR{
                    .format = static_cast<VkFormat>(SRGB_FORMAT), .colorSpace = static_cast<VkColorSpaceKHR>(vk::ColorSpaceKHR::eSrgbNonlinear)
                }
            )
            .set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eMailbox))
            .set_desired_extent(mWindowExtent.width, mWindowExtent.height)
            .add_image_usage_flags(static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eTransferDst))
            .set_desired_min_image_count(NUM_SWAPCHAIN_IMAGES)
            .set_create_flags(static_cast<VkSwapchainCreateFlagBitsKHR>(vk::SwapchainCreateFlagBitsKHR::eMutableFormat))
            .add_pNext(&formatListCreateInfo)
            .build()
            .value();
    mSwapchain = vk::raii::SwapchainKHR(*sRendererContext.mDevice, vkbSwapchain.swapchain);

    mImages.reserve(NUM_SWAPCHAIN_IMAGES);
    for (std::uint32_t i = 0; i < vkbSwapchain.get_images().value().size(); i++) {
        vk::ImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
        imageViewCreateInfo.image = vkbSwapchain.get_images().value()[i];
        imageViewCreateInfo.format = SRGB_FORMAT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        std::vector<vk::raii::ImageView> imageViews;
        imageViews.reserve(2);
        imageViews.emplace_back(sRendererContext.mDevice->createImageView(imageViewCreateInfo));
        imageViewCreateInfo.format = UNORM_FORMAT;
        imageViews.emplace_back(sRendererContext.mDevice->createImageView(imageViewCreateInfo));
        SwSwapchainImage swapchainImage(
            vkbSwapchain.get_images().value()[i], std::move(imageViews), SwSemaphoreFactory::createSemaphore(), formats, vk::Extent3D(vkbSwapchain.extent, 1)
        );
        mImages.emplace_back(std::move(swapchainImage));
    }

    mFrames.reserve(NUM_FRAME_OVERLAP);
    for (size_t i = 0; i < NUM_FRAME_OVERLAP; i++) {
        mFrames.emplace_back();
        mFrames.back().initialize();
    }

    mDrawImage = SwImageFactory::createColorImage2D(
        nullptr,
        vk::Extent3D{mWindowExtent, 1},
        DRAW_FORMAT,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eStorage,
        false
    );
    mDepthImage = SwImageFactory::createDepthImage2D(
        nullptr,
        mDrawImage.getExtent(),
        DEPTH_FORMAT,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        false
    );
    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < mImages.size(); i++) {
            mImages.at(i).emitTransition(
                cmd,
                vk::ImageLayout::ePresentSrcKHR,
                vk::PipelineStageFlagBits2::eNone,
                vk::AccessFlagBits2::eNone
            );
        }
        mDrawImage.emitTransition(cmd, vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        mDepthImage.emitTransition(
            cmd,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        );
    });
}

SwFrame& SwSwapchain::getCurrentFrame() {
    return mFrames.at(mFrameNumber % NUM_FRAME_OVERLAP);
}

SwFrame& SwSwapchain::getPreviousFrame() {
    return mFrames.at((mFrameNumber - 1) % NUM_FRAME_OVERLAP);
}

SwSwapchainImage& SwSwapchain::getCurrentSwapchainImage() {
    return mImages.at(mSwapchainIndex);
}

void SwSwapchain::acquireNextImage(uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence) {
    mSwapchainIndex = mSwapchain.acquireNextImage(1e9, getCurrentFrame().getAvailableSemaphore().getRawSemaphore(), nullptr).value;
}

SwSwapchain::~SwSwapchain() {
    mSwapchain.clear();
    mSurface.clear();
    SDL_DestroyWindow(mWindow);
}
