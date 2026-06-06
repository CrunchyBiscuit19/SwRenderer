#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwPipeline.h>

#include <vector>

struct SwPrimitive;

struct SwRenderItem {
    std::uint32_t mIndexCount;
    std::uint32_t mRInstCount;
    std::uint32_t mFirstIndex;
    std::uint32_t mVertexOffset;
    std::uint32_t mFirstRInst;
    std::uint32_t mMaterialIndex;
    std::uint32_t mNodeTransformIndex;
    std::uint32_t mAssetIndex;
    std::uint32_t mFirstInstance;
    std::uint32_t mBoundsIndex;
};

struct SwRenderInstance {
    std::uint32_t mRItemIndex;  // rename to: mRItemIndex
    std::uint32_t mSceneInstanceIndex;
};

class SwBatch {
private:
    static constexpr std::uint32_t RENDER_ITEMS_INITIAL_BUFFER_SIZE{sizeof(SwRenderItem) * (1 << 10)};
    static constexpr std::uint32_t RENDER_INSTANCES_INITIAL_BUFFER_SIZE{sizeof(SwRenderInstance) * (1 << 13)};

    SwGraphicsPipelineBundle* mGraphicsPipelineBundle{nullptr};
    bool mDoubleSided{false};

    std::vector<SwRenderItem> mRItems;
    SwStagingBuffer mRItemsStaging;
    SwAllocatedBuffer mInitialRItemsBuffer;
    SwAllocatedBuffer mOcclusionRItemsBuffer;
    SwAllocatedBuffer mOcclusionRItemsCount;

    std::vector<SwRenderInstance> mRInsts;
    SwStagingBuffer mRInstsStaging;
    SwAllocatedBuffer mRInstsBuffer;

public:
    static std::uint32_t sFirstRInstOffset;

    SwBatch() = default;
    SwBatch(SwPrimitive& primitive);

    SwBatch(SwBatch&&) noexcept = default;
    SwBatch& operator=(SwBatch&&) noexcept = default;

    SwBatch(const SwBatch&) = delete;
    SwBatch& operator=(const SwBatch&) = delete;

    inline SwGraphicsPipelineBundle& getGraphicsPipelineBundle() { return *mGraphicsPipelineBundle; }
    inline bool isDoubleSided() const { return mDoubleSided; }
    inline std::vector<SwRenderItem>& getRItems() { return mRItems; }
    inline std::vector<SwRenderInstance>& getRInsts() { return mRInsts; }
    inline SwStagingBuffer& getRItemsStaging() { return mRItemsStaging; }
    inline SwStagingBuffer& getRInstsStaging() { return mRInstsStaging; }
    inline SwAllocatedBuffer& getInitialRItemsBuffer() { return mInitialRItemsBuffer; }
    inline SwAllocatedBuffer& getOcclusionRItemsBuffer() { return mOcclusionRItemsBuffer; }
    inline SwAllocatedBuffer& getOcclusionRItemsCount() { return mOcclusionRItemsCount; }
    inline SwAllocatedBuffer& getFinalRItemsBuffer() { return mOcclusionRItemsBuffer; }  // Return whatever is the last one for future-proofing
    inline SwAllocatedBuffer& getFinalRItemsCount() { return mOcclusionRItemsCount; }  // Return whatever is the last one for future-proofing
    inline SwAllocatedBuffer& getRInstsBuffer() { return mRInstsBuffer; }
};
