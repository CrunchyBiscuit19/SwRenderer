#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Data/SwCamera.h>
#include <VkBootstrap.h>

SwRendererContext SwFrame::sRendererContext{};

SwFrame::SwFrame() : mCommandPool(nullptr), mCommandBuffer(nullptr), mRenderFence(nullptr), mAvailableSemaphore(nullptr) {}

void SwFrame::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

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

void SwFrame::update() {
    SwPerspective perspective = sRendererContext.mScene->getCamera().getPerspective();
    std::memcpy(mPerFrameBuffer.getMappedPointer(), &perspective, 1 * sizeof(SwPerspective));
}

SwRendererContext SwSwapchain::sRendererContext{};
vk::ClearColorValue SwSwapchain::DRAW_CLEAR_VALUE{.463f, .616f, .859f, 0.f};

SwSwapchain::SwSwapchain() : mSwapchain(nullptr), mSurface(nullptr) {}
 
void SwSwapchain::init(SwRendererContext swapchainContext) { sRendererContext = swapchainContext; }

void SwSwapchain::initialize(SDL_Window* window, vk::raii::SurfaceKHR surface, vk::Extent2D windowExtent, bool windowFullScreen) {
    mFrames.reserve(NUM_FRAME_OVERLAP);
    for (size_t i = 0; i < NUM_FRAME_OVERLAP; i++) {
        mFrames.emplace_back();
        mFrames.back().initialize();
    }

    sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
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
    vk::ImageFormatListCreateInfo formatListCreateInfo{};
    std::vector<vk::Format> formats = {SRGB_FORMAT, UNORM_FORMAT};
    formatListCreateInfo.pViewFormats = formats.data();
    formatListCreateInfo.viewFormatCount = formats.size();

    vkb::SwapchainBuilder swapchainBuilder(**sRendererContext.mChosenGPU, **sRendererContext.mDevice, *mSurface);
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
    mSwapchain = vk::raii::SwapchainKHR(*sRendererContext.mDevice, vkbSwapchain.swapchain);

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
        otherImageViews.emplace_back(sRendererContext.mDevice->createImageView(unormImageViewCreateInfo));

        SwSwapchainImage swapchainImage(
            vkbSwapchain.get_images().value()[i],
            formats[0],
            vk::Extent3D(vkbSwapchain.extent, 1),
            sRendererContext.mDevice->createImageView(srgbImageViewCreateInfo),
            SwSemaphoreFactory::createSemaphore(),
            {formats[1]},
            std::move(otherImageViews)
        );
        mSwapchainImages.emplace_back(std::move(swapchainImage));
    }

    mDrawImage = SwImageFactory::createColorImage2D(
        nullptr,
        DRAW_FORMAT,
        vk::Extent3D{mWindowExtent, 1},
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eStorage,
        false,
        vk::ClearColorValue(.463f, .616f, .859f, 0.f)
    );
    mDepthImage = SwImageFactory::createDepthImage2D(
        nullptr,
        DEPTH_FORMAT,
        mDrawImage.getExtent(),
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        false
    );
    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < mSwapchainImages.size(); i++) {
            mSwapchainImages[i].emitTransition(cmd, vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone);
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

void SwSwapchain::resize() {
    mDepthImage.destroy();
    mDrawImage.destroy();

    mSwapchain.clear();
    mSwapchainImages.clear();
}

SwSwapchainImage& SwSwapchain::getCurrentSwapchainImage() {
    return mSwapchainImages[mSwapchainIndex];
}

void SwSwapchain::acquireNextImage(uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence) {
    mSwapchainIndex = mSwapchain.acquireNextImage(1e9, getCurrentFrame().getAvailableSemaphore().getRawSemaphore(), nullptr).value;
}

SwSwapchain::~SwSwapchain() {
    mSwapchain.clear();
    mSurface.clear();
    SDL_DestroyWindow(mWindow);
}
