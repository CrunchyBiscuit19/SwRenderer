#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <vma/vk_mem_alloc.h>

#include <format>

std::uint32_t SwBatch::sFirstRiOffset = 0;

SwBatch::SwBatch(std::uint32_t pipelineId, std::size_t rcsOffset, std::size_t rcsSize, std::size_t risOffset, std::size_t risSize)
    : mPipelineId(pipelineId), mRcsOffset(rcsOffset), mRcsSize(rcsSize), mRisOffset(risOffset), mRisSize(risSize) {}
