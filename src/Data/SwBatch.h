#pragma once

#include <Data/SwMaterial.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwPipeline.h>

#include <functional>
#include <utility>
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
    std::uint32_t mBatchIndex;
};

struct SwRenderItem {
    std::uint32_t mRcIndex;
    std::uint32_t mSceneInstanceIndex;
};

class SwBatch {
private:
    std::uint32_t mBatchIndex{0};

    std::uint32_t mPipelineId{0};

    SwMaterial::Type mMaterialType;
    std::size_t mRcsIndex{0};
    std::size_t mRcsSize{0};
    std::size_t mRisIndex{0};
    std::size_t mRisSize{0};

public:
    struct Data {
        SwMaterial::Type mMaterialType;
        std::uint32_t mRcsIndex{0};
        std::uint32_t mRcsSize{0};
        std::uint32_t mRisIndex{0};
        std::uint32_t mRisSize{0};
    };

    static std::uint32_t sFirstRiOffset;

    SwBatch() = default;
    SwBatch(
        SwMaterial::Type materialType, std::uint32_t pipelineId, std::uint32_t batchIndex, std::size_t rcsIndex, std::size_t rcsSize, std::size_t risIndex,
        std::size_t risSize
    );

    SwBatch(SwBatch&&) noexcept = default;
    SwBatch& operator=(SwBatch&&) noexcept = default;

    SwBatch(const SwBatch&) = delete;
    SwBatch& operator=(const SwBatch&) = delete;

    inline SwGraphicsPipelineBundle& getGraphicsPipelineBundle() { return SwMaterial::getPipelineBundleById(mPipelineId); }

    inline std::uint32_t getBatchIndex() const { return mBatchIndex; }
    inline std::uint32_t getPipelineId() const { return mPipelineId; }
    inline SwMaterial::Type getMaterialType() const { return mMaterialType; }
    inline std::size_t getRcsIndex() const { return mRcsIndex; }
    inline std::size_t getRcsSize() const { return mRcsSize; }
    inline std::size_t getRisIndex() const { return mRisIndex; }
    inline std::size_t getRisSize() const { return mRisSize; }

    Data toData();
};

using SwBatchKey = std::pair<SwMaterial::Type, std::uint32_t>;

template <>
struct std::hash<SwBatchKey> {
    std::size_t operator()(const SwBatchKey& key) const noexcept {
        const std::size_t materialHash = std::hash<SwMaterial::Type>{}(key.first);
        const std::size_t pipelineHash = std::hash<std::uint32_t>{}(key.second);
        return materialHash ^ (pipelineHash + 0x9e3779b9 + (materialHash << 6) + (materialHash >> 2));
    }
};