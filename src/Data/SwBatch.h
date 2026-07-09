#pragma once

#include <Data/SwMaterial.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwPipeline.h>

#include <vector>

struct SwRenderCommand {
    std::uint32_t mIndexCount;
    std::uint32_t mRiCount;
    std::uint32_t mFirstIndex;
    std::uint32_t mVertexOffset;
    std::uint32_t mFirstRi;
    std::uint32_t mMaterialIndex;
    std::uint32_t mNodeTransformIndex;
    std::uint32_t mAssetIndex;
    std::uint32_t mFirstInstance;
    std::uint32_t mBoundsIndex;
    SwMaterial::Type mMaterialType;
};

struct SwRenderItem {
    std::uint32_t mRcIndex;
    std::uint32_t mSceneInstanceIndex;
};

class SwBatch {
private:
    std::uint32_t mPipelineId{0};

    std::size_t mRcsOffset{0};
    std::size_t mRcsSize{0};
    std::size_t mRisOffset{0};
    std::size_t mRisSize{0};
    /*vk::DeviceSize mInitialRcsBufferOffset{0}; // Maybe we won't need this
    vk::DeviceSize mInitialRcsBufferSize{0};
    vk::DeviceSize mInitialRcsLimitsBufferOffset{0};
    vk::DeviceSize mEarlyRcsBufferOffset{0};
    vk::DeviceSize mEarlyRcsBufferSize{0};
    vk::DeviceSize mEarlyRcsCountOffset{0};
    vk::DeviceSize mLateRcsBufferOffset{0};
    vk::DeviceSize mLateRcsBufferSize{0};
    vk::DeviceSize mLateRcsCountOffset{0};
    vk::DeviceSize mRisLimitsBufferOffset{0};
    vk::DeviceSize mMaterialTypesBufferOffset{0};*/

public:
    static std::uint32_t sFirstRiOffset;

    SwBatch() = default;
    SwBatch(std::uint32_t pipelineId, std::size_t rcsOffset, std::size_t rcsSize, std::size_t risOffset, std::size_t risSize);

    SwBatch(SwBatch&&) noexcept = default;
    SwBatch& operator=(SwBatch&&) noexcept = default;

    SwBatch(const SwBatch&) = delete;
    SwBatch& operator=(const SwBatch&) = delete;

    inline SwGraphicsPipelineBundle& getGraphicsPipelineBundle() { return SwMaterial::getPipelineBundleById(mPipelineId); }
};