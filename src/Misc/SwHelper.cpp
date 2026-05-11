#include <Misc/SwHelper.h>

#include <cmath>

vk::CommandPoolCreateInfo swHelper::commandPoolCreateInfo(std::uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags) {
    vk::CommandPoolCreateInfo info = {};
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

vk::CommandBufferAllocateInfo swHelper::commandBufferAllocateInfo(vk::CommandPool pool, std::uint32_t count) {
    vk::CommandBufferAllocateInfo info = {};
    info.pNext = nullptr;
    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = vk::CommandBufferLevel::ePrimary;
    return info;
}

vk::CommandBufferBeginInfo swHelper::commandBufferBeginInfo(vk::CommandBufferUsageFlags flags) {
    vk::CommandBufferBeginInfo info = {};
    info.pNext = nullptr;
    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

vk::FenceCreateInfo swHelper::fenceCreateInfo(vk::FenceCreateFlags flags) {
    vk::FenceCreateInfo info = {};
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

vk::SemaphoreCreateInfo swHelper::semaphoreCreateInfo() {
    vk::SemaphoreCreateInfo info = {};
    info.pNext = nullptr;
    return info;
}

vk::SemaphoreSubmitInfo swHelper::semaphoreSubmitInfo(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore) {
    vk::SemaphoreSubmitInfo submitInfo{};
    submitInfo.pNext = nullptr;
    submitInfo.semaphore = semaphore;
    submitInfo.stageMask = stageMask;
    submitInfo.deviceIndex = 0;
    submitInfo.value = 1;
    return submitInfo;
}

vk::CommandBufferSubmitInfo swHelper::commandBufferSubmitInfo(vk::CommandBuffer cmd) {
    vk::CommandBufferSubmitInfo info{};
    info.pNext = nullptr;
    info.commandBuffer = cmd;
    info.deviceMask = 0;
    return info;
}

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

vk::ImageSubresourceRange swHelper::imageSubresourceRange(vk::ImageAspectFlags aspectMask) {
    vk::ImageSubresourceRange subImage{};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;
    return subImage;
}

vk::DescriptorSetLayoutBinding swHelper::descriptorSetLayoutBinding(vk::DescriptorType type, vk::ShaderStageFlags stageFlags, std::uint32_t binding) {
    vk::DescriptorSetLayoutBinding setbind = {};
    setbind.binding = binding;
    setbind.descriptorCount = 1;
    setbind.descriptorType = type;
    setbind.pImmutableSamplers = nullptr;
    setbind.stageFlags = stageFlags;

    return setbind;
}

vk::DescriptorSetLayoutCreateInfo swHelper::descriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutBinding* bindings, std::uint32_t bindingCount) {
    vk::DescriptorSetLayoutCreateInfo info = {};
    info.pNext = nullptr;
    info.pBindings = bindings;
    info.bindingCount = bindingCount;
    // info.flags = vk::DescriptorSetLayoutCreateFlags::eDescriptorBufferEXT;
    return info;
}

vk::WriteDescriptorSet swHelper::writeDescriptorImage(vk::DescriptorType type, vk::DescriptorSet dstSet, vk::DescriptorImageInfo* imageInfo, std::uint32_t binding) {
    vk::WriteDescriptorSet write = {};
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.dstSet = dstSet;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = imageInfo;
    return write;
}

vk::WriteDescriptorSet swHelper::writeDescriptorBuffer(vk::DescriptorType type, vk::DescriptorSet dstSet, vk::DescriptorBufferInfo* bufferInfo, std::uint32_t binding) {
    vk::WriteDescriptorSet write = {};
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.dstSet = dstSet;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = bufferInfo;
    return write;
}

vk::DescriptorBufferInfo swHelper::bufferInfo(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range) {
    vk::DescriptorBufferInfo binfo{};
    binfo.buffer = buffer;
    binfo.offset = offset;
    binfo.range = range;
    return binfo;
}

vk::ImageCreateInfo swHelper::imageCreateInfo(vk::Format format, vk::ImageUsageFlags usageFlags, bool useMultisampling, vk::Extent3D extent) {
    vk::ImageCreateInfo info = {};
    info.pNext = nullptr;
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = extent;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    useMultisampling ? (info.samples = vk::SampleCountFlagBits::e4) : (info.samples = vk::SampleCountFlagBits::e1);
    info.tiling = vk::ImageTiling::eOptimal;  // Image is stored on the best gpu format
    info.usage = usageFlags;

    return info;
}

vk::ImageViewCreateInfo swHelper::imageViewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags) {
    vk::ImageViewCreateInfo info = {};
    info.pNext = nullptr;
    info.viewType = vk::ImageViewType::e2D;
    info.image = image;
    info.format = format;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.aspectMask = aspectFlags;
    return info;
}

void swHelper::transitionImage(
    vk::CommandBuffer cmd, vk::Image image, vk::ImageAspectFlags aspectFlags, vk::ImageLayout currentLayout, vk::PipelineStageFlags2 srcStageMask,
    vk::AccessFlags2 srcAccessMask, vk::ImageLayout newLayout, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask, std::uint32_t baseMipLevel
) {
    vk::ImageMemoryBarrier2 imageBarrier{};
    imageBarrier.pNext = nullptr;
    imageBarrier.srcStageMask = srcStageMask;
    imageBarrier.srcAccessMask = srcAccessMask;
    imageBarrier.dstStageMask = dstStageMask;
    imageBarrier.dstAccessMask = dstAccessMask;
    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;
    imageBarrier.subresourceRange = imageSubresourceRange(aspectFlags);
    imageBarrier.image = image;

    vk::DependencyInfo depInfo{};
    depInfo.pNext = nullptr;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    cmd.pipelineBarrier2(depInfo);
}

void swHelper::copyImage(vk::CommandBuffer cmd, vk::Image source, vk::Image destination, vk::Extent2D srcSize, vk::Extent2D dstSize) {
    vk::ImageBlit2 blitRegion{};
    blitRegion.pNext = nullptr;
    blitRegion.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    blitRegion.srcOffsets[1] = vk::Offset3D{static_cast<std::int32_t>(srcSize.width), static_cast<std::int32_t>(srcSize.height), 1};
    blitRegion.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    blitRegion.dstOffsets[1] = vk::Offset3D{static_cast<std::int32_t>(dstSize.width), static_cast<std::int32_t>(dstSize.height), 1};
    blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    vk::BlitImageInfo2 blitInfo{};
    blitInfo.pNext = nullptr;
    blitInfo.dstImage = destination;
    blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
    blitInfo.srcImage = source;
    blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
    blitInfo.filter = vk::Filter::eLinear;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    cmd.blitImage2(blitInfo);
}

void swHelper::generateMipmaps(vk::CommandBuffer cmd, SwAllocatedImage& allocatedImage, bool cubemap) {
    std::uint32_t numFaces = cubemap ? 6 : 1;
    const std::uint32_t mipLevels =
        static_cast<std::uint32_t>(std::floor(std::log2(std::max(allocatedImage.getExtent().width, allocatedImage.getExtent().height)))) + 1;
    vk::Extent2D imageSize = swHelper::extent3dTo2d(allocatedImage.getExtent());
    constexpr auto aspectMask = vk::ImageAspectFlagBits::eColor;

    for (std::uint32_t mip = 0; mip < mipLevels - 1; mip++) {
        // Transition current mipmap level to eTransferSrcOptimal
        vk::ImageMemoryBarrier2 mipBarrier{};
        mipBarrier.pNext = nullptr;
        mipBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        mipBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        mipBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        mipBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        mipBarrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
        mipBarrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
        mipBarrier.subresourceRange = imageSubresourceRange(aspectMask);
        mipBarrier.subresourceRange.baseArrayLayer = 0;
        mipBarrier.subresourceRange.layerCount = numFaces;
        mipBarrier.subresourceRange.baseMipLevel = mip;
        mipBarrier.subresourceRange.levelCount = 1;
        mipBarrier.image = allocatedImage.getRawImage();
        vk::DependencyInfo depInfo{};
        depInfo.pNext = nullptr;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &mipBarrier;
        cmd.pipelineBarrier2(depInfo);

        vk::Extent2D halfSize = vk::Extent2D(imageSize.width / 2, imageSize.height / 2);

        // Copy the image from previous level into next level at half resolution
        vk::ImageBlit2 blitRegion{};
        blitRegion.pNext = nullptr;
        blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = numFaces;
        blitRegion.srcSubresource.mipLevel = mip;
        blitRegion.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        blitRegion.srcOffsets[1] = vk::Offset3D{static_cast<std::int32_t>(imageSize.width), static_cast<std::int32_t>(imageSize.height), 1};
        blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = numFaces;
        blitRegion.dstSubresource.mipLevel = mip + 1;
        blitRegion.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        blitRegion.dstOffsets[1] = vk::Offset3D{static_cast<std::int32_t>(halfSize.width), static_cast<std::int32_t>(halfSize.height), 1};
        vk::BlitImageInfo2 blitInfo{};
        blitInfo.pNext = nullptr;
        blitInfo.srcImage = allocatedImage.getRawImage();
        blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
        blitInfo.dstImage = allocatedImage.getRawImage();
        blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
        blitInfo.filter = vk::Filter::eLinear;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;

        cmd.blitImage2(blitInfo);

        imageSize = halfSize;
    }

    // Final mip level transition
    vk::ImageMemoryBarrier2 mipBarrier{};
    mipBarrier.pNext = nullptr;
    mipBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    mipBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    mipBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
    mipBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    mipBarrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    mipBarrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
    mipBarrier.subresourceRange = imageSubresourceRange(aspectMask);
    mipBarrier.subresourceRange.baseArrayLayer = 0;
    mipBarrier.subresourceRange.layerCount = numFaces;
    mipBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
    mipBarrier.subresourceRange.levelCount = 1;
    mipBarrier.image = allocatedImage.getRawImage();
    vk::DependencyInfo depInfo{};
    depInfo.pNext = nullptr;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &mipBarrier;
    cmd.pipelineBarrier2(depInfo);

    // Update current layout, stage, access
    allocatedImage.setCurrentLayout(vk::ImageLayout::eTransferSrcOptimal);
    allocatedImage.setCurrentStage(vk::PipelineStageFlagBits2::eTransfer);
    allocatedImage.setCurrentAccess(vk::AccessFlagBits2::eTransferRead);
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

vk::PipelineLayoutCreateInfo swHelper::pipelineLayoutCreateInfo() {
    vk::PipelineLayoutCreateInfo info{};
    info.pNext = nullptr;
    // info.flags = vk::PipelineLayoutCreateFlags::eIndependentSetsEXT;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;
    return info;
}

vk::PipelineShaderStageCreateInfo swHelper::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule, const char* entry) {
    vk::PipelineShaderStageCreateInfo info{};
    info.pNext = nullptr;
    info.stage = stage;
    info.module = shaderModule;
    info.pName = entry;
    return info;
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

void swHelper::createBufferPipelineBarrier(
    vk::CommandBuffer cmd, vk::Buffer buffer, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask,
    vk::AccessFlags2 dstAccessMask
) {
    vk::BufferMemoryBarrier2 bufferBarrier{};
    bufferBarrier.pNext = nullptr;
    bufferBarrier.srcStageMask = srcStageMask;
    bufferBarrier.srcAccessMask = srcAccessMask;
    bufferBarrier.dstStageMask = dstStageMask;
    bufferBarrier.dstAccessMask = dstAccessMask;
    bufferBarrier.buffer = buffer;
    bufferBarrier.offset = 0;
    bufferBarrier.size = vk::WholeSize;

    vk::DependencyInfo depInfo{};
    depInfo.pNext = nullptr;
    depInfo.pBufferMemoryBarriers = &bufferBarrier;
    depInfo.bufferMemoryBarrierCount = 1;

    cmd.pipelineBarrier2(depInfo);
}

void swHelper::createImagePipelineBarrier(
    vk::CommandBuffer cmd, vk::Image image, vk::ImageAspectFlags aspectFlags, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask,
    vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask, vk::ImageLayout currentLayout
) {
    vk::ImageMemoryBarrier2 imageBarrier{};
    imageBarrier.pNext = nullptr;
    imageBarrier.srcStageMask = srcStageMask;
    imageBarrier.srcAccessMask = srcAccessMask;
    imageBarrier.dstStageMask = dstStageMask;
    imageBarrier.dstAccessMask = dstAccessMask;
    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = currentLayout;

    imageBarrier.subresourceRange = imageSubresourceRange(aspectFlags);
    imageBarrier.image = image;

    vk::DependencyInfo depInfo{};
    depInfo.pNext = nullptr;
    depInfo.pImageMemoryBarriers = &imageBarrier;
    depInfo.imageMemoryBarrierCount = 1;

    cmd.pipelineBarrier2(depInfo);
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
