#include <Renderer/SwRenderer.h>
#include <Renderer/SwStagingRing.h>
#include <Resource/SwImage.h>

#include <algorithm>
#include <memory>
#include <vector>

void SwStagingRing::initialize(std::uint64_t capacity) {
    const vk::PhysicalDeviceLimits& limits = SwRenderer::sRendererContext.mChosenGPU->getProperties().limits;
    mAlignment = std::max<std::uint64_t>({limits.optimalBufferCopyOffsetAlignment, limits.nonCoherentAtomSize, 4});
    mCapacity = alignUp(capacity, mAlignment);
    mRing = SwBufferFactory::createStagingBuffer("StagingRingBuffer", mCapacity, false, true);
    mHead = 0;
    mTail = 0;
    mInFlight.clear();
}

void SwStagingRing::destroy() {
    mRing.destroy();
    mInFlight.clear();
}

void SwStagingRing::grow(std::uint64_t requiredSize) {
    const std::uint64_t newCapacity = alignUp(std::max(requiredSize, mCapacity * 2), mAlignment);

    SwStagingBuffer newRing = SwBufferFactory::createStagingBuffer("StagingRingBuffer", newCapacity, false, true);

    // Already-recorded copies in the current frame still read the old buffer,
    // Defer destruction until every in-flight frame that references it has completed.
    SwBufferFactory::deferDestroy(std::make_unique<SwStagingBuffer>(std::move(mRing)));

    mRing = std::move(newRing);
    mCapacity = newCapacity;
    mHead = 0;
    mTail = 0;
    mInFlight.clear();
}

std::uint64_t SwStagingRing::reserve(std::uint64_t size) {
    std::uint64_t uploadSize = alignUp(size, mAlignment);

    std::uint64_t headOffset = mHead % mCapacity;
    bool skip = (headOffset + uploadSize > mCapacity);
    std::uint64_t skipSize = skip ? (mCapacity - headOffset) : 0;

    std::uint64_t regionStart = mHead + skipSize;
    std::uint64_t regionEnd = mHead + skipSize + uploadSize;
    if (regionEnd - mTail > mCapacity) {
        grow(uploadSize);
        regionStart = 0;
        regionEnd = uploadSize;
    }
    mHead = regionEnd;

    mInFlight.emplace_back(regionEnd, SwRenderer::sRendererContext.mSwapchain->getFrameNumber());
    return regionStart % mCapacity;
}

void SwStagingRing::upload(vk::CommandBuffer cmd, SwBuffer& dst, const void* src, std::uint64_t size, std::uint64_t dstOffset) {
    if (size == 0) {
        return;
    }
    std::uint64_t regionStartOffset = reserve(size);

    mRing.copyFromUnchecked(src, size, regionStartOffset);

    vk::BufferCopy copy{};
    copy.srcOffset = regionStartOffset;
    copy.dstOffset = dstOffset;
    copy.size = size;
    dst.copyFrom(cmd, mRing, copy);
}

void SwStagingRing::upload(vk::CommandBuffer cmd, SwImage& dst, const void* src, std::uint64_t size, vk::ArrayProxy<vk::BufferImageCopy> regions) {
    if (size == 0) {
        return;
    }
    std::uint64_t regionStartOffset = reserve(size);

    mRing.copyFromUnchecked(src, size, regionStartOffset);
    mRing.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
    dst.emitTransition(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal);

    std::vector<vk::BufferImageCopy> shiftedRegions(regions.begin(), regions.end());
    for (auto& region : shiftedRegions) {
        region.bufferOffset += regionStartOffset;
    }
    cmd.copyBufferToImage(mRing.getHandle(), dst.getHandle(), vk::ImageLayout::eTransferDstOptimal, shiftedRegions);
}

void SwStagingRing::tick(std::uint64_t currentFrame) {
    // Reclaim all regions with the current frame.
    // Keep popping and reassigning tail until the last one sets it as the new tail.
    while (!mInFlight.empty() && currentFrame >= mInFlight.front().mFrame + SwSwapchain::NUM_FRAME_OVERLAP) {
        mTail = mInFlight.front().mEnd;
        mInFlight.pop_front();
    }
}
