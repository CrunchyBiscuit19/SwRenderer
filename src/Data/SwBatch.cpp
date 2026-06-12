#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <fmt/core.h>
#include <vma/vk_mem_alloc.h>

std::uint32_t SwBatch::sFirstRiOffset = 0;

SwBatch::SwBatch(SwPrimitive& primitive) {
    mGraphicsPipelineBundle = &primitive.mMaterial.getPipelineBundle();
    mDoubleSided = primitive.mMaterial.isDoubleSided();

    const std::uint32_t batchId = mGraphicsPipelineBundle->getID();

    mRcsStaging = SwBufferFactory::createStagingBuffer(fmt::format("Batch{:<03}RcsStagingBuffer", batchId), RENDER_COMMANDS_INITIAL_BUFFER_SIZE);

    mInitialRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        fmt::format("Batch{:<03}InitialRcsBuffer", batchId),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_COMMANDS_INITIAL_BUFFER_SIZE,
        true
    );

    mEarlyRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        fmt::format("Batch{:<03}EarlyRcsBuffer", batchId),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_COMMANDS_INITIAL_BUFFER_SIZE,
        true
    );
    mEarlyRcsCount = SwBufferFactory::createAllocatedBuffer(
        fmt::format("Batch{:<03}EarlyRcsCountBuffer", batchId), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, sizeof(uint32_t), true
    );

    mLateRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        fmt::format("Batch{:<03}LateRcsBuffer", batchId),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_COMMANDS_INITIAL_BUFFER_SIZE,
        true
    );
    mLateRcsCount = SwBufferFactory::createAllocatedBuffer(
        fmt::format("Batch{:<03}LateRcsCountBuffer", batchId), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, sizeof(uint32_t), true
    );

    mRisStaging = SwBufferFactory::createStagingBuffer(fmt::format("Batch{:<03}RisStagingBuffer", batchId), RENDER_ITEMS_INITIAL_BUFFER_SIZE);
    mRisBuffer = SwBufferFactory::createAllocatedBuffer(
        fmt::format("Batch{:<03}RisBuffer", batchId), vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        RENDER_ITEMS_INITIAL_BUFFER_SIZE, true
    );
}
