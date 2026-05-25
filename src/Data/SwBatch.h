#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwPipeline.h>

#include <vector>

struct SwPrimitive;

struct SwRenderItem {
    std::uint32_t mIndexCount;
    std::uint32_t mRenderInstanceCount;
    std::uint32_t mFirstIndex;
    std::uint32_t mVertexOffset;
    std::uint32_t mFirstRenderInstance;
    std::uint32_t mMaterialIndex;
    std::uint32_t mNodeTransformIndex;
    std::uint32_t mAssetIndex;
    std::uint32_t mFirstInstance;
    std::uint32_t mBoundsIndex;
};

struct SwRenderInstance {
    std::uint32_t mRenderItemIndex;
    std::uint32_t mSceneInstanceIndex;
};

class SwBatch {
private:
    static constexpr std::uint32_t BATCH_MAX_RENDER_ITEMS{1 << 10};
    static constexpr std::uint32_t BATCH_MAX_RENDER_INSTANCES{BATCH_MAX_RENDER_ITEMS << 3};
    static constexpr std::uint32_t RENDER_ITEMS_BUFFER_SIZE{sizeof(SwRenderItem) * BATCH_MAX_RENDER_ITEMS};
    static constexpr std::uint32_t RENDER_INSTANCES_BUFFER_SIZE{sizeof(SwRenderInstance) * BATCH_MAX_RENDER_INSTANCES};

    SwGraphicsPipelineBundle* mGraphicsPipelineBundle{nullptr};

    std::vector<SwRenderItem> mRenderItems;
    SwStagingBuffer mRenderItemsStagingBuffer;
    SwAllocatedBuffer mPreCullRenderItemsBuffer;
    SwAllocatedBuffer mPostCullRenderItemsBuffer;
    SwAllocatedBuffer mPostCullRenderItemsCountBuffer;

    std::vector<SwRenderInstance> mRenderInstances;
    SwStagingBuffer mRenderInstancesStagingBuffer;
    SwAllocatedBuffer mRenderInstancesBuffer;

public:
    static std::uint32_t sFirstRenderInstanceOffset;

    SwBatch() = default;
    SwBatch(SwPrimitive& primitive);
    
    SwBatch(SwBatch&&) noexcept = default;
    SwBatch& operator=(SwBatch&&) noexcept = default;

    SwBatch(const SwBatch&) = delete;
    SwBatch& operator=(const SwBatch&) = delete;

    inline SwGraphicsPipelineBundle& getGraphicsPipelineBundle() { return *mGraphicsPipelineBundle; }
    inline std::vector<SwRenderItem>& getRenderItems() { return mRenderItems; }
    inline std::vector<SwRenderInstance>& getRenderInstances() { return mRenderInstances; }
    inline SwStagingBuffer& getRenderItemsStagingBuffer() { return mRenderItemsStagingBuffer; }
    inline SwStagingBuffer& getRenderInstancesStagingBuffer() { return mRenderInstancesStagingBuffer; }
    inline SwAllocatedBuffer& getPreCullRenderItemsBuffer() { return mPreCullRenderItemsBuffer; }
    inline SwAllocatedBuffer& getPostCullRenderItemsBuffer() { return mPostCullRenderItemsBuffer; }
    inline SwAllocatedBuffer& getPostCullRenderItemsCountBuffer() { return mPostCullRenderItemsCountBuffer; }
    inline SwAllocatedBuffer& getRenderInstancesBuffer() { return mRenderInstancesBuffer; }
};
