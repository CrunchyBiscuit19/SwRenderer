#pragma once

#include <Resource/SwImage.h>

#include <vulkan/vulkan.hpp>

namespace SwHelper {
vk::SubmitInfo2 submitInfo(vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo, vk::SemaphoreSubmitInfo* waitSemaphoreInfo);
vk::PresentInfoKHR presentInfo();

vk::Extent2D extent3dTo2d(vk::Extent3D extent3d);
std::uint32_t fastDivCeil(std::uint32_t x, std::uint32_t y);
std::uint32_t previousPow2(std::uint32_t value);
std::uint32_t nextPow2(std::uint32_t value);
std::uint32_t calculateMipMapLevels(vk::Extent3D extent);
}  // namespace swHelper
