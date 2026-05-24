#pragma once

#include <Resource/SwImage.h>

#include <vulkan/vulkan.hpp>

namespace swHelper {
vk::SubmitInfo2 submitInfo(vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo, vk::SemaphoreSubmitInfo* waitSemaphoreInfo);
vk::PresentInfoKHR presentInfo();

std::uint32_t getFormatTexelSize(vk::Format format);
void setViewportScissors(vk::CommandBuffer cmd, vk::Extent3D drawImageExtent);
vk::Extent2D extent3dTo2d(vk::Extent3D extent3d);

std::uint32_t fastCeil(std::uint32_t x, std::uint32_t y);
std::uint32_t previousPow2(std::uint32_t value);
std::uint32_t nextPow2(std::uint32_t value);
std::uint32_t calculateMipMapLevels(vk::Extent3D extent);
}  // namespace swHelper
