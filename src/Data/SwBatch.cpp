#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <vma/vk_mem_alloc.h>

std::uint32_t SwBatch::sFirstRiOffset = 0;

SwBatch::SwBatch(SwPrimitive& primitive) {
    mGraphicsPipelineBundle = &primitive.mMaterial.getPipelineBundle();
    mDoubleSided = primitive.mMaterial.isDoubleSided();

    mRcsStaging = SwBufferFactory::createStagingBuffer(RENDER_COMMANDS_INITIAL_BUFFER_SIZE);

    mInitialRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_COMMANDS_INITIAL_BUFFER_SIZE,
        true
    );

    mEarlyRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_COMMANDS_INITIAL_BUFFER_SIZE,
        true
    );
    mEarlyRcsCount = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, sizeof(uint32_t), true
    );

    mLateRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_COMMANDS_INITIAL_BUFFER_SIZE,
        true
    );
    mLateRcsCount = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, sizeof(uint32_t), true
    );

    mRisStaging = SwBufferFactory::createStagingBuffer(RENDER_ITEMS_INITIAL_BUFFER_SIZE);
    mRisBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, RENDER_ITEMS_INITIAL_BUFFER_SIZE, true
    );
}
