#pragma once

#include <Data/SwAsset.h>
#include <Data/SwBatch.h>
#include <Data/SwCamera.h>
#include <Pass/SwCull.h>
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
private:
    static std::filesystem::path CULL_RESET_COMPUTE_SHADER_PATH;
    static std::filesystem::path CULL_WORK_COMPUTE_SHADER_PATH;
    static std::filesystem::path CULL_COMPACT_COMPUTE_SHADER_PATH;
    static std::filesystem::path CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH;
    static const std::uint32_t CULL_MAX_DEPTH_PYRAMID_LEVELS = 16;
    
    static SwRendererContext sRendererContext;

    // --- Camera and Assets ---
    SwCamera mCamera;
    std::unordered_map<std::string, SwAsset> mAssets;
    
    // --- Batches ---
    std::unordered_map<std::uint32_t, SwBatch> mOpaqueBatches;
    std::unordered_map<std::uint32_t, SwBatch> mMaskBatches;
    std::unordered_map<std::uint32_t, SwBatch> mTransparentBatches;

    // --- Passes and Resources ---
    std::unordered_map<std::string, SwPass> mPasses;
    SwCull::Resources mCullResources;

    // --- Scene --- 
    SwDescriptorSet mSceneMaterialResourcesDescriptorSet;
    SwDescriptorLayout mSceneMaterialResourcesDescriptorLayout;
    SwAllocatedBuffer mSceneVertexBuffer;
    SwAllocatedBuffer mSceneIndexBuffer;
    SwAllocatedBuffer mSceneNodeTransformsBuffer;
    SwAllocatedBuffer mSceneMaterialConstantsBuffer;
    SwAllocatedBuffer mSceneInstancesBuffer;
    SwAllocatedBuffer mSceneBoundsBuffer;
    SwAllocatedBuffer mSceneVisibleRenderInstancesInstanceIndexBuffer;
    
    // --- Render graph ---
    SwRenderGraph mRenderGraph;

    void initializeCullResources();

    void reInitializableCullResources();

public:
    SwSceneFlags mFlags;

    static void init(SwRendererContext rendererContext);

    void initialize();

    void writeOnResize();
};
