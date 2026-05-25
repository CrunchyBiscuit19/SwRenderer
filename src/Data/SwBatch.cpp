#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <vma/vk_mem_alloc.h>

std::uint32_t SwBatch::sFirstRenderInstanceOffset = 0;

SwBatch::SwBatch(SwPrimitive& primitive) {
    mGraphicsPipelineBundle = &primitive.mMaterial.getPipelineBundle();

    mRenderItemsStagingBuffer = SwBufferFactory::createStagingBuffer(RENDER_ITEMS_BUFFER_SIZE);
    mPreCullRenderItemsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_ITEMS_BUFFER_SIZE
    );
    mPostCullRenderItemsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_ITEMS_BUFFER_SIZE
    );
    mPostCullRenderItemsCountBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        sizeof(uint32_t)
    );

    mRenderInstancesStagingBuffer = SwBufferFactory::createStagingBuffer(RENDER_INSTANCES_BUFFER_SIZE);
    mRenderInstancesBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_INSTANCES_BUFFER_SIZE
    );
}
