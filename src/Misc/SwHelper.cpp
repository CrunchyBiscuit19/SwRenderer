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
