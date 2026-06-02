#include <Renderer/SwStats.h>

SwStats::SwStats() {}

void SwStats::initialize() {
    mRenderInstancesCountBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, sizeof(std::uint32_t), true
    );
}

void SwStats::perFrameReset() {
    mDrawCallCount = 0;
    mPreCullRenderInstancesCount = 0;
}
