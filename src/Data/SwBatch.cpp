#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <vma/vk_mem_alloc.h>

#include <format>

std::uint32_t SwBatch::sFirstRiOffset = 0;

SwBatch::SwBatch(SwMaterial& material) {
    mPipelineId = material.getPipelineBundle().getID();
    mDoubleSided = material.isDoubleSided();

    const std::uint32_t batchId = mPipelineId;

    mInitialRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        std::format("Batch{:<03}InitialRcsBuffer", batchId),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        RENDER_COMMANDS_INITIAL_BUFFER_SIZE,
        true
    );

    mEarlyRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        std::format("Batch{:<03}EarlyRcsBuffer", batchId),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        RENDER_COMMANDS_INITIAL_BUFFER_SIZE,
        true
    );
    mEarlyRcsCount = SwBufferFactory::createAllocatedBuffer(
        std::format("Batch{:<03}EarlyRcsCountBuffer", batchId),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        sizeof(uint32_t),
        true
    );

    mLateRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        std::format("Batch{:<03}LateRcsBuffer", batchId),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        RENDER_COMMANDS_INITIAL_BUFFER_SIZE,
        true
    );
    mLateRcsCount = SwBufferFactory::createAllocatedBuffer(
        std::format("Batch{:<03}LateRcsCountBuffer", batchId),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        sizeof(uint32_t),
        true
    );

    mRisBuffer = SwBufferFactory::createAllocatedBuffer(
        std::format("Batch{:<03}RisBuffer", batchId), vk::BufferUsageFlagBits::eStorageBuffer, 0, RENDER_ITEMS_INITIAL_BUFFER_SIZE, true
    );
}
