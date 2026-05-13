#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <VkBootstrap.h>

SwFrame::SwFrame() : mCommandPool(nullptr), mCommandBuffer(nullptr), mRenderFence(nullptr), mAvailableSemaphore(nullptr) {}

void SwFrame::initialize() {
    mPerspectiveBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        sizeof(std::uint32_t)  // TODO
    );
}

SwSwapchainContext SwSwapchain::sSwapchainContext{};

SwSwapchain::SwSwapchain() : mSwapchain(nullptr) {}

void SwSwapchain::init(SwSwapchainContext swapchainContext) { sSwapchainContext = swapchainContext; }

void SwSwapchain::initialize() {
    vk::ImageFormatListCreateInfo formatListCreateInfo{};
    std::array<vk::Format, 2> formats = {vk::Format::eB8G8R8A8Srgb, vk::Format::eB8G8R8A8Unorm};
    formatListCreateInfo.pViewFormats = formats.data();
    formatListCreateInfo.viewFormatCount = formats.size();

    vkb::SwapchainBuilder swapchainBuilder(**sSwapchainContext.mChosenGPU, **sSwapchainContext.mDevice, **sSwapchainContext.mSurface);
    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            .set_desired_format(
                VkSurfaceFormatKHR{.format = static_cast<VkFormat>(formats[0]), .colorSpace = static_cast<VkColorSpaceKHR>(vk::ColorSpaceKHR::eSrgbNonlinear)
                }
            )
            .set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eMailbox))
            .set_desired_extent(sSwapchainContext.mWindowExtent.width, sSwapchainContext.mWindowExtent.height)
            .add_image_usage_flags(static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eTransferDst))
            .set_desired_min_image_count(NUM_SWAPCHAIN_IMAGES)
            .set_create_flags(static_cast<VkSwapchainCreateFlagBitsKHR>(vk::SwapchainCreateFlagBitsKHR::eMutableFormat))
            .add_pNext(&formatListCreateInfo)
            .build()
            .value();
    mSwapchain = vk::raii::SwapchainKHR(*sSwapchainContext.mDevice, vkbSwapchain.swapchain);

    //mSwapchainBundle.mExtent = vkbSwapchain.extent;
}