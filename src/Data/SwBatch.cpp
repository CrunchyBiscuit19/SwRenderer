#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <vma/vk_mem_alloc.h>

std::uint32_t SwBatch::sFirstRenderInstanceOffset = 0;

SwBatch::SwBatch(SwPrimitive& primitive) {
    mGraphicsPipelineBundle = &primitive.mMaterial.getPipelineBundle();

    mRenderItemsStagingBuffer = SwBufferFactory::createStagingBuffer(RENDER_ITEMS_INITIAL_BUFFER_SIZE);
    mPreCullRenderItemsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_ITEMS_INITIAL_BUFFER_SIZE,
        true
    );
    mPostCullRenderItemsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_ITEMS_INITIAL_BUFFER_SIZE,
        true
    );
    mPostCullRenderItemsCountBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        sizeof(uint32_t),
        true
    );

    mRenderInstancesStagingBuffer = SwBufferFactory::createStagingBuffer(RENDER_INSTANCES_INITIAL_BUFFER_SIZE);
    mRenderInstancesBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_INSTANCES_INITIAL_BUFFER_SIZE,
        true
    );
}
