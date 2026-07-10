#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <vma/vk_mem_alloc.h>

#include <format>

std::uint32_t SwBatch::sFirstRiOffset = 0;

SwBatch::SwBatch(
    SwMaterial::Type materialType, std::uint32_t pipelineId, std::uint32_t batchIndex, std::size_t rcsIndex, std::size_t rcsSize, std::size_t risIndex,
    std::size_t risSize
)
    : mBatchIndex(batchIndex),
      mPipelineId(pipelineId),
      mMaterialType(materialType),
      mRcsIndex(rcsIndex),
      mRcsSize(rcsSize),
      mRisIndex(risIndex),
      mRisSize(risSize) {}

SwBatch::Data SwBatch::toData() {
    return {
        mMaterialType, 
        static_cast<std::uint32_t>(mRcsIndex),
        static_cast<std::uint32_t>(mRcsSize),
        static_cast<std::uint32_t>(mRisIndex),
        static_cast<std::uint32_t>(mRisSize)
    };
}
