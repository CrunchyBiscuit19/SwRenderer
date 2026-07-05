#include <Renderer/SwRenderer.h>
#include <Renderer/SwStagingRing.h>

#include <algorithm>
#include <memory>

void SwStagingRing::initialize(std::uint64_t capacity) {
    const vk::PhysicalDeviceLimits& limits = SwRenderer::sRendererContext.mChosenGPU->getProperties().limits;
    mAlignment = std::max<std::uint64_t>({limits.optimalBufferCopyOffsetAlignment, limits.nonCoherentAtomSize, 4});
    mCapacity = alignUp(capacity, mAlignment);
    mRing = SwBufferFactory::createStagingBuffer("StagingRingBuffer", mCapacity, false);
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

    SwStagingBuffer newRing = SwBufferFactory::createStagingBuffer("StagingRingBuffer", newCapacity, false);

    // Already-recorded copies in the current frame still read the old buffer, 
    // Defer destruction until every in-flight frame that references it has completed.
    SwBufferFactory::deferDestroy(std::make_unique<SwStagingBuffer>(std::move(mRing)));

    mRing = std::move(newRing);
    mCapacity = newCapacity;
    mHead = 0;
    mTail = 0;
    mInFlight.clear();
}

void SwStagingRing::upload(vk::CommandBuffer cmd, SwBuffer& dst, const void* src, std::uint64_t size, std::uint64_t dstOffset) {
    if (size == 0) {
        return;
    }

    size = alignUp(size, mAlignment);

    std::uint64_t start = mHead % mCapacity;
    std::uint64_t skip = (start + size > mCapacity) ? (mCapacity - start) : 0;
    std::uint64_t regionEnd = mHead + skip + size;

    if (regionEnd - mTail > mCapacity) {
        grow(size);
        skip = 0;
        regionEnd = size;
    }

    const std::uint64_t srcOffset = (mHead + skip) % mCapacity;

    mRing.copyFromUnchecked(src, size, srcOffset);

    vk::BufferCopy copy{};
    copy.srcOffset = srcOffset;
    copy.dstOffset = dstOffset;
    copy.size = size;
    dst.copyFrom(cmd, mRing, copy);

    mHead = regionEnd;
    mInFlight.emplace_back(mHead, SwRenderer::sRendererContext.mSwapchain->getFrameNumber());
}

void SwStagingRing::tick(std::uint64_t currentFrame) {
    while (!mInFlight.empty() && currentFrame >= mInFlight.front().mFrame + SwSwapchain::NUM_FRAME_OVERLAP) {
        mTail = mInFlight.front().mHeadEnd;
        mInFlight.pop_front();
    }
}
