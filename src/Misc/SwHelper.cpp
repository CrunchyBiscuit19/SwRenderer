#include <Misc/SwHelper.h>

#include <cmath>


vk::SubmitInfo2 swHelper::submitInfo(
    vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo, vk::SemaphoreSubmitInfo* waitSemaphoreInfo
) {
    vk::SubmitInfo2 info = {};
    info.pNext = nullptr;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;

    return info;
}

vk::PresentInfoKHR swHelper::presentInfo() {
    vk::PresentInfoKHR info = {};
    info.pNext = nullptr;
    info.swapchainCount = 0;
    info.pSwapchains = nullptr;
    info.pWaitSemaphores = nullptr;
    info.waitSemaphoreCount = 0;
    info.pImageIndices = nullptr;
    return info;
}

vk::RenderingAttachmentInfo swHelper::colorAttachmentInfo(
    vk::ImageView view, vk::ImageLayout layout, vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp, std::optional<vk::ImageView> resolveImageView
) {
    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.pNext = nullptr;
    colorAttachment.imageView = view;
    colorAttachment.imageLayout = layout;
    colorAttachment.loadOp = loadOp;
    colorAttachment.storeOp = storeOp;
    //colorAttachment.clearValue = CLEAR_COLOR;
    if (resolveImageView.has_value()) {
        colorAttachment.resolveImageView = resolveImageView.value();
        colorAttachment.resolveMode = vk::ResolveModeFlagBits::eAverage;
        colorAttachment.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    }

    return colorAttachment;
}

vk::RenderingAttachmentInfo swHelper::depthAttachmentInfo(
    vk::ImageView view, vk::ImageLayout layout, vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp, std::optional<vk::ImageView> resolveImageView
) {
    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.pNext = nullptr;
    depthAttachment.imageView = view;
    depthAttachment.imageLayout = layout;
    depthAttachment.loadOp = loadOp;
    depthAttachment.storeOp = storeOp;
    depthAttachment.clearValue.depthStencil.depth = 0.f;
    if (resolveImageView.has_value()) {
        depthAttachment.resolveImageView = resolveImageView.value();
        depthAttachment.resolveMode = vk::ResolveModeFlagBits::eSampleZero;
        depthAttachment.resolveImageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    }

    return depthAttachment;
}

vk::RenderingInfo swHelper::renderingInfo(
    vk::Extent2D renderExtent, vk::RenderingAttachmentInfo* colorAttachment, vk::RenderingAttachmentInfo* depthAttachment, std::uint32_t count
) {
    vk::RenderingInfo renderInfo{};
    renderInfo.pNext = nullptr;
    renderInfo.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, renderExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = count;
    renderInfo.pColorAttachments = colorAttachment;
    renderInfo.pDepthAttachment = depthAttachment;
    renderInfo.pStencilAttachment = nullptr;
    return renderInfo;
}

std::uint32_t swHelper::getFormatTexelSize(vk::Format format) {
    std::uint32_t bytesPerTexel = 0;
    switch (format) {
        case vk::Format::eR8G8B8A8Srgb:
        case vk::Format::eR8G8B8A8Unorm:
            bytesPerTexel = 4;
            break;
        default:
            break;
    }

    return bytesPerTexel;
}

void swHelper::setViewportScissors(vk::CommandBuffer cmd, vk::Extent3D drawImageExtent) {
    vk::Extent2D drawImage2dExtent = extent3dTo2d(drawImageExtent);

    vk::Viewport viewport = {
        0,
        0,
        static_cast<float>(drawImage2dExtent.width),
        static_cast<float>(drawImage2dExtent.height),
        0.f,
        1.f,
    };
    cmd.setViewport(0, viewport);
    vk::Rect2D scissor = {
        vk::Offset2D{0, 0},
        drawImage2dExtent,
    };
    cmd.setScissor(0, scissor);
}

vk::Extent2D swHelper::extent3dTo2d(vk::Extent3D extent3d) { return vk::Extent2D(extent3d.width, extent3d.height); }

std::uint32_t swHelper::fastCeil(std::uint32_t x, std::uint32_t y) { return (x + y - 1) / y; }

std::uint32_t swHelper::previousPow2(std::uint32_t x) {
    if (x == 0) return 0;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x - (x >> 1);
}

std::uint32_t swHelper::nextPow2(std::uint32_t x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

std::uint32_t swHelper::calculateMipMapLevels(vk::Extent3D extent) { return std::floor(std::log2(std::max(extent.width, extent.height))) + 1; }
