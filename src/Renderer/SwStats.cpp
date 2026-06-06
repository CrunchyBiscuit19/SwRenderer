#include <Renderer/SwStats.h>

SwStats::SwStats() {}

void SwStats::initialize() {
    // Scratch: storage (atomic accumulation) + transfer source (copied into the published buffer).
    mRInstsScratchCount = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, sizeof(std::uint32_t),
        true
    );
    // Published: transfer destination, host-readable by the GUI.
    mRInstsPublishedCount = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, sizeof(std::uint32_t)
    );
}

void SwStats::perFrameReset() {
    mNumDrawCall = 0;
    mNumInitialRInsts = 0;
}
