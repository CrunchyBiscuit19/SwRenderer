#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwPipeline.h>

#include <vector>

struct SwPrimitive;

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
};

struct SwRenderItem {
    std::uint32_t mRcIndex;  // rename to: mRcIndex
    std::uint32_t mSceneInstanceIndex;
};

class SwBatch {
private:
    SwGraphicsPipelineBundle* mGraphicsPipelineBundle{nullptr};
    bool mDoubleSided{false};

    std::vector<SwRenderCommand> mRcs;
    SwStagingBuffer mRcsStaging;
    SwAllocatedBuffer mInitialRcsBuffer;
    SwAllocatedBuffer mEarlyRcsBuffer; // Compacted early draw list 
    SwAllocatedBuffer mEarlyRcsCount;   
    SwAllocatedBuffer mLateRcsBuffer;
    SwAllocatedBuffer mLateRcsCount;

    std::vector<SwRenderItem> mRis;
    SwStagingBuffer mRisStaging;
    SwAllocatedBuffer mRisBuffer;

public:
    static constexpr std::uint32_t RENDER_COMMANDS_INITIAL_BUFFER_SIZE{sizeof(SwRenderCommand) * (1 << 10)};
    static constexpr std::uint32_t RENDER_ITEMS_INITIAL_BUFFER_SIZE{sizeof(SwRenderItem) * (1 << 13)};

    static std::uint32_t sFirstRiOffset;

    SwBatch() = default;
    SwBatch(SwPrimitive& primitive);

    SwBatch(SwBatch&&) noexcept = default;
    SwBatch& operator=(SwBatch&&) noexcept = default;

    SwBatch(const SwBatch&) = delete;
    SwBatch& operator=(const SwBatch&) = delete;

    inline SwGraphicsPipelineBundle& getGraphicsPipelineBundle() { return *mGraphicsPipelineBundle; }
    inline bool isDoubleSided() const { return mDoubleSided; }
    
    inline std::vector<SwRenderCommand>& getRcs() { return mRcs; }
    inline SwStagingBuffer& getRcsStaging() { return mRcsStaging; }
    inline SwAllocatedBuffer& getInitialRcsBuffer() { return mInitialRcsBuffer; }
    inline SwAllocatedBuffer& getEarlyRcsBuffer() { return mEarlyRcsBuffer; }
    inline SwAllocatedBuffer& getEarlyRcsCount() { return mEarlyRcsCount; }
    inline SwAllocatedBuffer& getLateRcsBuffer() { return mLateRcsBuffer; }
    inline SwAllocatedBuffer& getLateRcsCount() { return mLateRcsCount; }
    inline SwAllocatedBuffer& getFinalRcsBuffer() { return mLateRcsBuffer; }  // Return whatever is the last one for future-proofing
    inline SwAllocatedBuffer& getFinalRcsCount() { return mLateRcsCount; }  // Return whatever is the last one for future-proofing

    inline std::vector<SwRenderItem>& getRis() { return mRis; }
    inline SwStagingBuffer& getRisStaging() { return mRisStaging; }
    inline SwAllocatedBuffer& getRisBuffer() { return mRisBuffer; }
};
