#include <Misc/SwHelper.h>
#include <Renderer/SwRenderer.h>
#include <Resource/SwImage.h>

SwImage::SwImage() {}

SwImage::SwImage(vk::Format mainFormat, vk::Extent3D extent, vk::ImageAspectFlags aspect, std::vector<vk::Format> otherFormats)
    : mMainFormat(mainFormat),
      mOtherFormats(std::move(otherFormats)),
      mExtent(extent),
      mCurrentLayout(vk::ImageLayout::eUndefined),
      mCurrentStage(vk::PipelineStageFlagBits2::eTopOfPipe),
      mCurrentAccess(vk::AccessFlags2()),
      mAspect(aspect) {}

void SwImage::emitTransition(vk::CommandBuffer cmd, SwDependency::ImageDepType imageDepType) {
    emitTransition(
        cmd,
        SwDependency::ImageDepDesc::get(imageDepType).mStage,
        SwDependency::ImageDepDesc::get(imageDepType).mAccess,
        SwDependency::ImageDepDesc::get(imageDepType).mLayout
    );
}

void SwImage::copyFrom(vk::CommandBuffer cmd, SwImage& source) {
    copyFrom(cmd, source.getRawImage(), SwHelper::extent3dTo2d(source.getExtent()), vk::ImageAspectFlagBits::eColor);
}

vk::RenderingAttachmentInfo SwImage::generateRenderingAttachment(vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp) {
    vk::RenderingAttachmentInfo renderingAttachment{};
    renderingAttachment.pNext = nullptr;
    renderingAttachment.imageView = this->getRawMainImageView();
    renderingAttachment.imageLayout = mCurrentLayout;
    renderingAttachment.loadOp = loadOp;
    renderingAttachment.storeOp = storeOp;
    renderingAttachment.clearValue = this->getClearValue();
    return renderingAttachment;
}

vk::RenderingAttachmentInfo SwImage::generateRenderingAttachment(
    std::uint32_t otherImageViewIndex, vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp
) {
    vk::RenderingAttachmentInfo renderingAttachment{};
    renderingAttachment.pNext = nullptr;
    renderingAttachment.imageView = this->getRawOtherImageView(otherImageViewIndex);
    renderingAttachment.imageLayout = mCurrentLayout;
    renderingAttachment.loadOp = loadOp;
    renderingAttachment.storeOp = storeOp;
    renderingAttachment.clearValue = this->getClearValue();
    return renderingAttachment;
}

SwSwapchainImage::SwSwapchainImage()
    : SwImage(), mImage(VK_NULL_HANDLE), mMainImageView(nullptr) {}

SwSwapchainImage::SwSwapchainImage(
    vk::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, SwSemaphore renderedSemaphore,
    std::vector<vk::Format> otherFormats, std::deque<vk::raii::ImageView> otherImageViews
)
    : SwImage(mainFormat, extent, vk::ImageAspectFlagBits::eColor, std::move(otherFormats)),
      mImage(image),
      mMainImageView(std::move(mainImageView)),
      mOtherImageViews(std::move(otherImageViews)),
      mRenderedSemaphore(std::move(renderedSemaphore)) {}

void SwSwapchainImage::emitBarrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess) {
    emitTransition(cmd, nextStage, nextAccess, mCurrentLayout);
}

void SwSwapchainImage::emitTransition(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess, vk::ImageLayout nextLayout) {
    static constexpr vk::AccessFlags2 kAnyWrite =
        vk::AccessFlagBits2::eShaderWrite |
        vk::AccessFlagBits2::eColorAttachmentWrite |
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite |
        vk::AccessFlagBits2::eTransferWrite |
        vk::AccessFlagBits2::eHostWrite |
        vk::AccessFlagBits2::eMemoryWrite |
        vk::AccessFlagBits2::eShaderStorageWrite;
    if (!(mCurrentAccess & kAnyWrite) && nextLayout == mCurrentLayout && nextStage == mCurrentStage && nextAccess == mCurrentAccess) {
        return;
    }

    vk::ImageMemoryBarrier2 barrierInfo = {};
    barrierInfo.srcStageMask = mCurrentStage;
    barrierInfo.dstStageMask = nextStage;
    barrierInfo.srcAccessMask = mCurrentAccess;
    barrierInfo.dstAccessMask = nextAccess;
    barrierInfo.oldLayout = mCurrentLayout;
    barrierInfo.newLayout = nextLayout;
    barrierInfo.image = mImage;
    barrierInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrierInfo.subresourceRange.baseMipLevel = 0;
    barrierInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrierInfo.subresourceRange.baseArrayLayer = 0;
    barrierInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    barrierInfo.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierInfo.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vk::DependencyInfo depInfo{};
    depInfo.pNext = nullptr;
    depInfo.pImageMemoryBarriers = &barrierInfo;
    depInfo.imageMemoryBarrierCount = 1;
    cmd.pipelineBarrier2(depInfo);

    mCurrentStage = nextStage;
    mCurrentAccess = nextAccess;
    mCurrentLayout = nextLayout;
}

void SwSwapchainImage::copyFrom(vk::CommandBuffer cmd, vk::Image source, vk::Extent2D srcSize, vk::ImageAspectFlags srcAspect) {
    vk::ImageBlit2 blitRegion{};
    blitRegion.pNext = nullptr;
    blitRegion.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    blitRegion.srcOffsets[1] = vk::Offset3D{static_cast<std::int32_t>(srcSize.width), static_cast<std::int32_t>(srcSize.height), 1};
    blitRegion.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    blitRegion.dstOffsets[1] = vk::Offset3D{static_cast<std::int32_t>(mExtent.width), static_cast<std::int32_t>(mExtent.height), 1};
    blitRegion.srcSubresource.aspectMask = srcAspect;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.dstSubresource.aspectMask = mAspect;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    vk::BlitImageInfo2 blitInfo{};
    blitInfo.pNext = nullptr;
    blitInfo.dstImage = mImage;
    blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
    blitInfo.srcImage = source;
    blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
    blitInfo.filter = vk::Filter::eLinear;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    cmd.blitImage2(blitInfo);
}

SwAllocatedImage::SwAllocatedImage() : mImage(nullptr), mMainImageView(nullptr), mAllocation(nullptr), mAllocator(nullptr), mMipLevels(1), mMipmapped(false) {}

SwAllocatedImage::SwAllocatedImage(
    vk::raii::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, vk::ImageUsageFlags usage, vk::ClearValue clearValue,
    vk::ImageAspectFlags aspect, const VmaAllocator allocator, VmaAllocation allocation, bool mipmapped, std::vector<vk::Format> otherFormats,
    std::deque<vk::raii::ImageView> otherImageViews
)
    : SwImage(mainFormat, extent, aspect, std::move(otherFormats)),
      mImage(std::move(image)),
      mMainImageView(std::move(mainImageView)),
      mOtherImageViews(std::move(otherImageViews)),
      mClearValue(clearValue),
      mUsage(usage),
      mAllocator(allocator),
      mAllocation(allocation),
      mMipmapped(mipmapped),
      mMipLevels(mipmapped ? SwHelper::calculateMipMapLevels(extent) : 1) {}

void SwAllocatedImage::emitBarrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess) {
    emitTransition(cmd, nextStage, nextAccess, mCurrentLayout);
}

void SwAllocatedImage::emitTransition(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess, vk::ImageLayout nextLayout) {
    static constexpr vk::AccessFlags2 kAnyWrite =
        vk::AccessFlagBits2::eShaderWrite |
        vk::AccessFlagBits2::eColorAttachmentWrite |
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite |
        vk::AccessFlagBits2::eTransferWrite |
        vk::AccessFlagBits2::eHostWrite |
        vk::AccessFlagBits2::eMemoryWrite |
        vk::AccessFlagBits2::eShaderStorageWrite;
    if (!(mCurrentAccess & kAnyWrite) && nextLayout == mCurrentLayout && nextStage == mCurrentStage && nextAccess == mCurrentAccess) {
        return;
    }

    vk::ImageMemoryBarrier2 barrierInfo = {};
    barrierInfo.srcStageMask = mCurrentStage;
    barrierInfo.dstStageMask = nextStage;
    barrierInfo.srcAccessMask = mCurrentAccess;
    barrierInfo.dstAccessMask = nextAccess;
    barrierInfo.oldLayout = mCurrentLayout;
    barrierInfo.newLayout = nextLayout;
    barrierInfo.image = *mImage;
    barrierInfo.subresourceRange.aspectMask = mAspect;
    barrierInfo.subresourceRange.baseMipLevel = 0;
    barrierInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrierInfo.subresourceRange.baseArrayLayer = 0;
    barrierInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    barrierInfo.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierInfo.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vk::DependencyInfo depInfo{};
    depInfo.pNext = nullptr;
    depInfo.pImageMemoryBarriers = &barrierInfo;
    depInfo.imageMemoryBarrierCount = 1;
    cmd.pipelineBarrier2(depInfo);

    mCurrentStage = nextStage;
    mCurrentAccess = nextAccess;
    mCurrentLayout = nextLayout;
}

void SwAllocatedImage::copyFrom(vk::CommandBuffer cmd, vk::Image source, vk::Extent2D srcSize, vk::ImageAspectFlags srcAspect) {
    vk::ImageBlit2 blitRegion{};
    blitRegion.pNext = nullptr;
    blitRegion.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    blitRegion.srcOffsets[1] = vk::Offset3D{static_cast<std::int32_t>(srcSize.width), static_cast<std::int32_t>(srcSize.height), 1};
    blitRegion.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    blitRegion.dstOffsets[1] = vk::Offset3D{static_cast<std::int32_t>(mExtent.width), static_cast<std::int32_t>(mExtent.height), 1};
    blitRegion.srcSubresource.aspectMask = srcAspect;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.dstSubresource.aspectMask = mAspect;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    vk::BlitImageInfo2 blitInfo{};
    blitInfo.pNext = nullptr;
    blitInfo.dstImage = *mImage;
    blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
    blitInfo.srcImage = source;
    blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
    blitInfo.filter = vk::Filter::eLinear;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    cmd.blitImage2(blitInfo);
}

void SwAllocatedImage::generateMipmaps(vk::CommandBuffer cmd, std::uint32_t numFaces) {
    const std::uint32_t mipLevels = SwHelper::calculateMipMapLevels(mExtent);
    vk::Extent2D imageSize = SwHelper::extent3dTo2d(mExtent);
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
        mipBarrier.subresourceRange.aspectMask = aspectMask;
        mipBarrier.subresourceRange.baseArrayLayer = 0;
        mipBarrier.subresourceRange.layerCount = numFaces;
        mipBarrier.subresourceRange.baseMipLevel = mip;
        mipBarrier.subresourceRange.levelCount = 1;
        mipBarrier.image = *mImage;
        vk::DependencyInfo depInfo{};
        depInfo.pNext = nullptr;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &mipBarrier;
        cmd.pipelineBarrier2(depInfo);

        vk::Extent2D halfSize = vk::Extent2D(std::max(1u, imageSize.width / 2), std::max(1u, imageSize.height / 2));

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
        blitInfo.srcImage = *mImage;
        blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
        blitInfo.dstImage = *mImage;
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
    mipBarrier.subresourceRange.aspectMask = aspectMask;
    mipBarrier.subresourceRange.baseArrayLayer = 0;
    mipBarrier.subresourceRange.layerCount = numFaces;
    mipBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
    mipBarrier.subresourceRange.levelCount = 1;
    mipBarrier.image = *mImage;
    vk::DependencyInfo depInfo{};
    depInfo.pNext = nullptr;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &mipBarrier;
    cmd.pipelineBarrier2(depInfo);

    // Update current layout, stage, access
    mCurrentLayout = vk::ImageLayout::eTransferSrcOptimal;
    mCurrentStage = vk::PipelineStageFlagBits2::eTransfer;
    mCurrentAccess = vk::AccessFlagBits2::eTransferRead;
}

void SwAllocatedImage::addImageView(
    vk::Format format, vk::ImageAspectFlags aspect, vk::ImageViewType viewType, std::uint32_t baseMipLevel, std::uint32_t levelCount,
    std::uint32_t baseArrayLayer, std::uint32_t layerCount
) {
    mOtherImageViews.emplace_back(
        std::move(SwImageFactory::createImageView(*mImage, format, aspect, viewType, baseMipLevel, levelCount, baseArrayLayer, layerCount))
    );
}

void SwAllocatedImage::destroy() {
    if (!isReady()) {
        return;
    }
    mMainImageView.clear();
    mOtherImageViews.clear();
    vk::Image rawImage = mImage.release();
    vmaDestroyImage(mAllocator, rawImage, mAllocation);
    mAllocator = nullptr;
    mAllocation = nullptr;
}

SwAllocatedImage::SwAllocatedImage(SwAllocatedImage&& other) noexcept
    : SwImage(std::move(other)),
      mImage(std::move(other.mImage)),
      mMainImageView(std::move(other.mMainImageView)),
      mOtherImageViews(std::move(other.mOtherImageViews)),
      mClearValue(other.mClearValue),
      mMipLevels(other.mMipLevels),
      mMipmapped(other.mMipmapped),
      mAllocator(other.mAllocator),
      mAllocation(other.mAllocation) {
    other.mAllocator = nullptr;
    other.mAllocation = nullptr;
}

SwAllocatedImage& SwAllocatedImage::operator=(SwAllocatedImage&& other) noexcept {
    if (this != &other) {
        destroy();

        SwImage::operator=(std::move(other));
        mImage = std::move(other.mImage);
        mMainImageView = std::move(other.mMainImageView);
        mOtherImageViews = std::move(other.mOtherImageViews);
        mClearValue = other.mClearValue;
        mMipLevels = other.mMipLevels;
        mMipmapped = other.mMipmapped;
        mAllocator = other.mAllocator;
        mAllocation = other.mAllocation;

        other.mAllocator = nullptr;
        other.mAllocation = nullptr;
    }
    return *this;
}

SwAllocatedImage::~SwAllocatedImage() { destroy(); }

SwColorImage2D::SwColorImage2D() {}

SwColorImage2D::SwColorImage2D(
    vk::raii::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, vk::ImageUsageFlags usage, vk::ClearValue clearValue,
    const VmaAllocator allocator, VmaAllocation allocation, bool mipmapped, std::vector<vk::Format> otherFormats,
    std::deque<vk::raii::ImageView> otherImageViews
)
    : SwAllocatedImage(
          std::move(image), mainFormat, extent, std::move(mainImageView), usage, clearValue, vk::ImageAspectFlagBits::eColor, allocator, allocation, mipmapped,
          std::move(otherFormats), std::move(otherImageViews)
      ) {}

void SwColorImage2D::generateMipmaps(vk::CommandBuffer cmd) { SwAllocatedImage::generateMipmaps(cmd, 1); }

void SwColorImage2D::resize(vk::Extent3D newExtent) {
    *this = SwImageFactory::createColorImage2D(nullptr, mMainFormat, newExtent, mUsage, mMipmapped, mClearValue);
}

SwDepthImage2D::SwDepthImage2D() {}

SwDepthImage2D::SwDepthImage2D(
    vk::raii::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, vk::ImageUsageFlags usage, vk::ClearValue clearValue,
    const VmaAllocator allocator, VmaAllocation allocation, bool mipmapped, std::vector<vk::Format> otherFormats,
    std::deque<vk::raii::ImageView> otherImageViews
)
    : SwAllocatedImage(
          std::move(image), mainFormat, extent, std::move(mainImageView), usage, clearValue, vk::ImageAspectFlagBits::eDepth, allocator, allocation, mipmapped,
          std::move(otherFormats), std::move(otherImageViews)
      ) {
    mClearValue.depthStencil.depth = 0.f;
}

void SwDepthImage2D::generateMipmaps(vk::CommandBuffer cmd) { SwAllocatedImage::generateMipmaps(cmd, 1); }

void SwDepthImage2D::resize(vk::Extent3D newExtent) {
    *this = SwImageFactory::createDepthImage2D(nullptr, mMainFormat, newExtent, mUsage, mMipmapped, mClearValue);
}

SwColorImageCubemap::SwColorImageCubemap() {}

SwColorImageCubemap::SwColorImageCubemap(
    vk::raii::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, vk::ImageUsageFlags usage, vk::ClearValue clearValue,
    const VmaAllocator allocator, VmaAllocation allocation, bool mipmapped, std::vector<vk::Format> otherFormats,
    std::deque<vk::raii::ImageView> otherImageViews
)
    : SwAllocatedImage(
          std::move(image), mainFormat, extent, std::move(mainImageView), usage, clearValue, vk::ImageAspectFlagBits::eColor, allocator, allocation, mipmapped,
          std::move(otherFormats), std::move(otherImageViews)
      ) {}

void SwColorImageCubemap::generateMipmaps(vk::CommandBuffer cmd) { SwAllocatedImage::generateMipmaps(cmd, SwImageFactory::NUM_CUBEMAP_FACES); }

void SwColorImageCubemap::resize(vk::Extent3D newExtent) {
    *this = SwImageFactory::createColorImageCubemap(nullptr, mMainFormat, newExtent, mUsage, mMipmapped, mClearValue);
}

SwStagingBuffer SwImageFactory::sImageStagingBuffer;
std::unordered_map<SwImageFactory::SwDefaultImageOption, SwColorImage2D> SwImageFactory::sDefaultImages;

std::uint32_t SwImageFactory::getFormatTexelSize(vk::Format format) {
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

SwImageFactory::SwImageConstructionInfo SwImageFactory::prepareImageConstructionInfo(
    SwImageType swImageType, const void* data, vk::Format mainFormat, vk::Extent3D extent, vk::ImageUsageFlags usage, bool mipmapped, vk::ClearValue clearValue
) {
    vk::ImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.pNext = nullptr;
    imageCreateInfo.imageType = vk::ImageType::e2D;
    imageCreateInfo.format = mainFormat;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.extent = extent;
    imageCreateInfo.mipLevels = mipmapped ? SwHelper::calculateMipMapLevels(extent) : 1;
    imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    imageCreateInfo.usage = usage;
    switch (swImageType) {
        case SwImageType::SwColorImage2D:
            break;
        case SwImageType::SwDepthImage2D:
            assert(mainFormat == vk::Format::eD32Sfloat || mainFormat == vk::Format::eD24UnormS8Uint);
            break;
        case SwImageType::SwColorImageCubemap:
            imageCreateInfo.arrayLayers = NUM_CUBEMAP_FACES;
            imageCreateInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
            break;
    };
    VkImageCreateInfo imageCreateInfo1 = imageCreateInfo;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkImage tempImage;
    VmaAllocation tempAllocation;
    auto result = vmaCreateImage(SwRenderer::sRendererContext.mAllocator, &imageCreateInfo1, &vmaAllocInfo, &tempImage, &tempAllocation, nullptr);

    vk::ImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.pNext = nullptr;
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.image = tempImage;
    imageViewCreateInfo.format = mainFormat;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    switch (swImageType) {
        case SwImageType::SwColorImage2D:
            break;
        case SwImageType::SwDepthImage2D:
            imageViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
            break;
        case SwImageType::SwColorImageCubemap:
            imageViewCreateInfo.viewType = vk::ImageViewType::eCube;
            imageViewCreateInfo.subresourceRange.layerCount = NUM_CUBEMAP_FACES;
            break;
    }

    return SwImageConstructionInfo{
        vk::raii::Image(*SwRenderer::sRendererContext.mDevice, tempImage),
        vk::raii::ImageView(*SwRenderer::sRendererContext.mDevice, imageViewCreateInfo),
        mainFormat,
        extent,
        mipmapped,
        usage,
        clearValue,
        SwRenderer::sRendererContext.mAllocator,
        std::move(tempAllocation)
    };
}

void SwImageFactory::fillImageData(SwImageType swImageType, const void* data, SwAllocatedImage& image) {
    std::uint32_t numFaces = 1;
    switch (swImageType) {
        case SwImageType::SwColorImage2D:
        case SwImageType::SwDepthImage2D:
            break;
        case SwImageType::SwColorImageCubemap:
            numFaces = NUM_CUBEMAP_FACES;
            break;
    }

    std::uint32_t bytesPerTexel = getFormatTexelSize(image.getMainFormat());
    const size_t faceSize = image.getExtent().depth * image.getExtent().width * image.getExtent().height * bytesPerTexel;
    const size_t dataSize = faceSize * numFaces;
    SwRenderer::sRendererContext.mImmSubmit->individualSubmit([&](vk::CommandBuffer cmd) {
        sImageStagingBuffer.copyFrom(cmd, data, dataSize);
        sImageStagingBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        image.emitTransition(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal);

        std::vector<vk::BufferImageCopy> copyRegions;
        copyRegions.reserve(numFaces);
        for (std::uint32_t face = 0; face < numFaces; face++) {
            vk::BufferImageCopy copyRegion;
            copyRegion.bufferOffset = face * faceSize;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = face;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = image.getExtent();
            copyRegions.emplace_back(copyRegion);
        }

        cmd.copyBufferToImage(sImageStagingBuffer.getRawBuffer(), image.getRawImage(), vk::ImageLayout::eTransferDstOptimal, copyRegions);

        if (image.isMipmapped()) image.generateMipmaps(cmd);

        image.emitTransition(cmd, vk::PipelineStageFlagBits2KHR::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal);
    });
}

void SwImageFactory::init() {
    sImageStagingBuffer = SwBufferFactory::createStagingBuffer(IMAGE_STAGING_BUFFER_SIZE);
    constexpr std::uint32_t white = std::byteswap(0xFFFFFFFF);
    sDefaultImages.try_emplace(
        SwDefaultImageOption::White,
        SwImageFactory::createColorImage2D(&white, vk::Format::eR8G8B8A8Srgb, vk::Extent3D{1, 1, 1}, vk::ImageUsageFlagBits::eSampled, false)
    );
    constexpr std::uint32_t grey = std::byteswap(0xAAAAAAFF);
    sDefaultImages.try_emplace(
        SwDefaultImageOption::Grey,
        SwImageFactory::createColorImage2D(&grey, vk::Format::eR8G8B8A8Srgb, vk::Extent3D{1, 1, 1}, vk::ImageUsageFlagBits::eSampled, false)
    );
    constexpr std::uint32_t black = std::byteswap(0x000000FF);
    sDefaultImages.try_emplace(
        SwDefaultImageOption::Black,
        SwImageFactory::createColorImage2D(&black, vk::Format::eR8G8B8A8Srgb, vk::Extent3D{1, 1, 1}, vk::ImageUsageFlagBits::eSampled, false)
    );
    constexpr std::uint32_t blue = std::byteswap(0x769DDBFF);
    sDefaultImages.try_emplace(
        SwDefaultImageOption::Blue,
        SwImageFactory::createColorImage2D(&blue, vk::Format::eR8G8B8A8Srgb, vk::Extent3D{1, 1, 1}, vk::ImageUsageFlagBits::eSampled, false)
    );
    std::array<std::uint32_t, 16 * 16> pixels;
    for (std::uint32_t x = 0; x < 16; x++) {
        for (std::uint32_t y = 0; y < 16; y++) {
            constexpr std::uint32_t magenta = std::byteswap(0xFF00FFFF);
            pixels[static_cast<std::array<std::uint32_t, 256Ui64>::size_type>(y) * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    sDefaultImages.try_emplace(
        SwDefaultImageOption::Checkerboard,
        SwImageFactory::createColorImage2D(pixels.data(), vk::Format::eR8G8B8A8Srgb, vk::Extent3D{16, 16, 1}, vk::ImageUsageFlagBits::eSampled, false)
    );
}

vk::raii::ImageView SwImageFactory::createImageView(
    vk::Image image, vk::Format format, vk::ImageAspectFlags aspect, vk::ImageViewType viewType, std::uint32_t baseMipLevel, std::uint32_t levelCount,
    std::uint32_t baseArrayLayer, std::uint32_t layerCount
) {
    vk::ImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.pNext = nullptr;
    imageViewCreateInfo.viewType = viewType;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.baseMipLevel = baseMipLevel;
    imageViewCreateInfo.subresourceRange.levelCount = levelCount;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
    imageViewCreateInfo.subresourceRange.layerCount = layerCount;
    imageViewCreateInfo.subresourceRange.aspectMask = aspect;
    return SwRenderer::sRendererContext.mDevice->createImageView(imageViewCreateInfo);
}

SwColorImage2D SwImageFactory::createColorImage2D(
    const void* data, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags usage, bool mipmapped, vk::ClearValue clearValue
) {
    if (data != nullptr) usage |= vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
    SwImageConstructionInfo imageConstructionInfo =
        prepareImageConstructionInfo(SwImageType::SwColorImage2D, data, format, extent, usage, mipmapped, clearValue);
    SwColorImage2D newImage = SwColorImage2D(
        std::move(imageConstructionInfo.mImage),
        imageConstructionInfo.mMainFormat,
        imageConstructionInfo.mExtent,
        std::move(imageConstructionInfo.mMainImageView),
        imageConstructionInfo.mUsage,
        imageConstructionInfo.mClearValue,
        imageConstructionInfo.mAllocator,
        std::move(imageConstructionInfo.mAllocation),
        imageConstructionInfo.mMipmapped
    );
    if (data != nullptr) fillImageData(SwImageType::SwColorImage2D, data, newImage);
    return newImage;
}

SwDepthImage2D SwImageFactory::createDepthImage2D(
    const void* data, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags usage, bool mipmapped, vk::ClearValue clearValue
) {
    if (data != nullptr) usage |= vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
    SwImageConstructionInfo imageConstructionInfo =
        prepareImageConstructionInfo(SwImageType::SwDepthImage2D, data, format, extent, usage, mipmapped, clearValue);
    SwDepthImage2D newImage = SwDepthImage2D(
        std::move(imageConstructionInfo.mImage),
        imageConstructionInfo.mMainFormat,
        imageConstructionInfo.mExtent,
        std::move(imageConstructionInfo.mMainImageView),
        imageConstructionInfo.mUsage,
        imageConstructionInfo.mClearValue,
        imageConstructionInfo.mAllocator,
        std::move(imageConstructionInfo.mAllocation),
        imageConstructionInfo.mMipmapped
    );
    if (data != nullptr) fillImageData(SwImageType::SwDepthImage2D, data, newImage);
    return newImage;
}

SwColorImageCubemap SwImageFactory::createColorImageCubemap(
    const void* data, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags usage, bool mipmapped, vk::ClearValue clearValue
) {
    if (data != nullptr) usage |= vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
    SwImageConstructionInfo imageConstructionInfo =
        prepareImageConstructionInfo(SwImageType::SwColorImageCubemap, data, format, extent, usage, mipmapped, clearValue);
    SwColorImageCubemap newImage = SwColorImageCubemap(
        std::move(imageConstructionInfo.mImage),
        imageConstructionInfo.mMainFormat,
        imageConstructionInfo.mExtent,
        std::move(imageConstructionInfo.mMainImageView),
        imageConstructionInfo.mUsage,
        imageConstructionInfo.mClearValue,
        imageConstructionInfo.mAllocator,
        std::move(imageConstructionInfo.mAllocation),
        imageConstructionInfo.mMipmapped
    );
    if (data != nullptr) fillImageData(SwImageType::SwColorImageCubemap, data, newImage);
    return newImage;
}

void SwImageFactory::cleanup() {
    sDefaultImages.clear();
    sImageStagingBuffer.destroy();
}
