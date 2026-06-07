#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <vma/vk_mem_alloc.h>

std::uint32_t SwBatch::sFirstRInstOffset = 0;

SwBatch::SwBatch(SwPrimitive& primitive) {
    mGraphicsPipelineBundle = &primitive.mMaterial.getPipelineBundle();
    mDoubleSided = primitive.mMaterial.isDoubleSided();

    mRItemsStaging = SwBufferFactory::createStagingBuffer(RENDER_ITEMS_INITIAL_BUFFER_SIZE);

    mInitialRItemsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_ITEMS_INITIAL_BUFFER_SIZE,
        true
    );

    mEarlyRItemsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_ITEMS_INITIAL_BUFFER_SIZE,
        true
    );
    mEarlyRItemsCount = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, sizeof(uint32_t), true
    );

    mLateRItemsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_ITEMS_INITIAL_BUFFER_SIZE,
        true
    );
    mLateRItemsCount = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, sizeof(uint32_t), true
    );

    mRInstsStaging = SwBufferFactory::createStagingBuffer(RENDER_INSTANCES_INITIAL_BUFFER_SIZE);
    mRInstsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, RENDER_INSTANCES_INITIAL_BUFFER_SIZE, true
    );
}
