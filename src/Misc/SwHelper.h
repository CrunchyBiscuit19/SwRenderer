#pragma once

#include <Resource/SwImage.h>
#include <vulkan/vulkan.hpp>

namespace swHelper {
vk::SubmitInfo2 submitInfo(vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo, vk::SemaphoreSubmitInfo* waitSemaphoreInfo);
vk::PresentInfoKHR presentInfo();

vk::RenderingAttachmentInfo colorAttachmentInfo(
    vk::ImageView view, vk::ImageLayout layout, vk::AttachmentLoadOp load = vk::AttachmentLoadOp::eLoad,
    vk::AttachmentStoreOp store = vk::AttachmentStoreOp::eStore, std::optional<vk::ImageView> swapchainResolveView = std::nullopt
);
vk::RenderingAttachmentInfo depthAttachmentInfo(
    vk::ImageView view, vk::ImageLayout layout, vk::AttachmentLoadOp load = vk::AttachmentLoadOp::eLoad,
    vk::AttachmentStoreOp store = vk::AttachmentStoreOp::eStore, std::optional<vk::ImageView> swapchainResolveView = std::nullopt
);
vk::RenderingInfo renderingInfo(
    vk::Extent2D renderExtent, vk::RenderingAttachmentInfo* colorAttachment, vk::RenderingAttachmentInfo* depthAttachment, std::uint32_t count = 1
);

std::uint32_t getFormatTexelSize(vk::Format format);

void setViewportScissors(vk::CommandBuffer cmd, vk::Extent3D drawImageExtent);

vk::Extent2D extent3dTo2d(vk::Extent3D extent3d);

std::uint32_t fastCeil(std::uint32_t x, std::uint32_t y);

std::uint32_t previousPow2(std::uint32_t value);
std::uint32_t nextPow2(std::uint32_t value);

std::uint32_t calculateMipMapLevels(vk::Extent3D extent);
} 
