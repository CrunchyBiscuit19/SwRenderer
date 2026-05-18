#pragma once

#include <Data/SwAsset.h>
#include <Data/SwBatch.h>
#include <Data/SwCamera.h>
#include <Pass/SwPass.h>
#include <Pass/SwRenderGraph.h>
#include <Resource/SwDescriptor.h>

struct SwSceneFlags {
    bool mAssetLoadedFlag;
    bool mAssetUnloadedFlag;
    bool mInstanceLoadedFlag;
    bool mInstanceUnloadedFlag;
    bool mReloadMainBufferFlag;
};

class SwScene {
public:
    SwSceneFlags mFlags;

private:
    SwCamera mCamera;
    std::unordered_map<std::string, SwAsset> mAssets;
    std::vector<SwPass> mPasses;
    SwRenderGraph mRenderGraph;
    std::unordered_map<std::uint32_t, SwBatch> mOpaqueBatches;
    std::unordered_map<std::uint32_t, SwBatch> mMaskBatches;
    std::unordered_map<std::uint32_t, SwBatch> mTransparentBatches;
    SwDescriptorSet mSceneMaterialResourcesDescriptorSet;
    SwDescriptorLayout mSceneMaterialResourcesDescriptorLayout;
    SwAllocatedBuffer mSceneVertexBuffer;
    SwAllocatedBuffer mSceneIndexBuffer;
    SwAllocatedBuffer mSceneNodeTransformsBuffer;
    SwAllocatedBuffer mSceneMaterialConstantsBuffer;
    SwAllocatedBuffer mSceneInstancesBuffer;
    SwAllocatedBuffer mSceneBoundsBuffer;
    SwAllocatedBuffer mSceneVisibleRenderInstancesInstanceIndexBuffer;
};
