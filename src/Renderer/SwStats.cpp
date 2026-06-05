#include <Renderer/SwStats.h>

SwStats::SwStats() {}

void SwStats::initialize() {
    mRInstsCount = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, sizeof(std::uint32_t), true
    );
}

void SwStats::perFrameReset() {
    mNumDrawCall = 0;
    mNumInitialRInsts = 0;
}
