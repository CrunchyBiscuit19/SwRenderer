#pragma once

#include <Resources/SwImage.h>
#include <vulkan/vulkan.hpp>

namespace swHelper {
vk::CommandPoolCreateInfo commandPoolCreateInfo(std::uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags);
vk::CommandBufferAllocateInfo commandBufferAllocateInfo(vk::CommandPool pool, std::uint32_t count = 1);
vk::CommandBufferBeginInfo commandBufferBeginInfo(vk::CommandBufferUsageFlags flags);
vk::CommandBufferSubmitInfo commandBufferSubmitInfo(vk::CommandBuffer cmd);

vk::FenceCreateInfo fenceCreateInfo(vk::FenceCreateFlags flags);
vk::SemaphoreCreateInfo semaphoreCreateInfo();
vk::SemaphoreSubmitInfo semaphoreSubmitInfo(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore);

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

vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding(vk::DescriptorType type, vk::ShaderStageFlags stageFlags, std::uint32_t binding);
vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutBinding* bindings, std::uint32_t bindingCount);
vk::WriteDescriptorSet writeDescriptorImage(vk::DescriptorType type, vk::DescriptorSet dstSet, vk::DescriptorImageInfo* imageInfo, std::uint32_t binding);
vk::WriteDescriptorSet writeDescriptorBuffer(vk::DescriptorType type, vk::DescriptorSet dstSet, vk::DescriptorBufferInfo* bufferInfo, std::uint32_t binding);
vk::DescriptorBufferInfo bufferInfo(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range);

vk::ImageSubresourceRange imageSubresourceRange(vk::ImageAspectFlags aspectMask);
vk::ImageCreateInfo imageCreateInfo(vk::Format format, vk::ImageUsageFlags usageFlags, bool multisampling, vk::Extent3D extent);
vk::ImageViewCreateInfo imageViewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags);

void transitionImage(
    vk::CommandBuffer cmd, vk::Image image, vk::ImageAspectFlags aspectFlags, vk::ImageLayout currentLayout, vk::PipelineStageFlags2 srcStageMask,
    vk::AccessFlags2 srcAccessMask, vk::ImageLayout newLayout, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask, std::uint32_t baseMipLevel = 0
);
void copyImage(vk::CommandBuffer cmd, vk::Image source, vk::Image destination, vk::Extent2D srcSize, vk::Extent2D dstSize);
void generateMipmaps(vk::CommandBuffer cmd, SwAllocatedImage& image, bool cubemap = false);
std::uint32_t getFormatTexelSize(vk::Format format);

vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo();
vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule, const char* entry = "main");
void setViewportScissors(vk::CommandBuffer cmd, vk::Extent3D drawImageExtent);

void createBufferPipelineBarrier(
    vk::CommandBuffer cmd, vk::Buffer buffer, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask,
    vk::AccessFlags2 dstAccessMask
);
void createImagePipelineBarrier(
    vk::CommandBuffer cmd, vk::Image image, vk::ImageAspectFlags aspectFlags, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask,
    vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask, vk::ImageLayout currentLayout
);

vk::Extent2D extent3dTo2d(vk::Extent3D extent3d);

std::uint32_t fastCeil(std::uint32_t x, std::uint32_t y);

std::uint32_t previousPow2(std::uint32_t value);
std::uint32_t nextPow2(std::uint32_t value);

std::uint32_t calculateMipMapLevels(vk::Extent3D extent);
} 
