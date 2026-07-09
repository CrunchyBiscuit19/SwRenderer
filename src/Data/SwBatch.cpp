#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <vma/vk_mem_alloc.h>

#include <format>

std::uint32_t SwBatch::sFirstRiOffset = 0;

SwBatch::SwBatch(std::uint32_t pipelineId, std::uint32_t batchIndex, std::size_t rcsIndex, std::size_t rcsSize, std::size_t risIndex, std::size_t risSize)
    : mPipelineId(pipelineId), mBatchIndex(batchIndex), mRcsIndex(rcsIndex), mRcsSize(rcsSize), mRisIndex(risIndex), mRisSize(risSize) {}
