#include <Renderer/SwStats.h>

SwStats::SwStats() {}

void SwStats::initialize() {
    mRenderInstancesCountBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        sizeof(std::uint32_t)
    );
}

void SwStats::reset() {
    mDrawCallCount = 0;
    mPreCullRenderInstancesCount = 0;
}
