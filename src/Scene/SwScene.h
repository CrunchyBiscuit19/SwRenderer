
#pragma once

#include <Data/SwAsset.h>
#include <Data/SwBatch.h>
#include <Data/SwCamera.h>
#include <Data/SwMaterial.h>
#include <Resource/SwDescriptor.h>
#include <Scene/SwPass.h>
#include <Scene/SwRenderGraph.h>
#include <System/SwCull.h>
#include <System/SwGeometry.h>
#include <System/SwGui.h>
#include <System/SwIBL.h>
#include <System/SwInput.h>
#include <System/SwLighting.h>
#include <System/SwPick.h>
#include <System/SwPostProcess.h>
#include <System/SwWBOIT.h>

#include <algorithm>
#include <array>
#include <ranges>
#include <set>
#include <span>
#include <unordered_set>

class SwScene {
public:
    struct Flags {
        bool mAssetLoaded{false};
        bool mAssetUnloaded{false};
        bool mInstanceLoaded{false};
        bool mInstanceUnloaded{false};
        bool mReloadMainInstancesBuffer{false};
        bool mLightEdited{false};
    };

private:
    friend class SwCull::System;
    friend class SwPick::System;
    friend class SwIBL::System;
    friend class SwWBOIT::System;
    friend class SwGeometry::System;
    friend class SwPostProcess::System;
    friend class SwLighting::System;
    friend class SwGui::System;

    SwCamera mCamera;

    std::unordered_map<std::uint32_t, SwAsset> mAssets;
    std::unordered_set<std::string> mAlreadyLoadedAssetsNames;
    std::unordered_set<std::uint32_t> mAssetsIdsToFill;  // loaded this frame
    std::unordered_set<std::uint32_t> mAssetsIdsToFree;  // uploaded last frame

    std::unordered_map<std::uint32_t, SwInstance> mInstances;

    std::unordered_map<std::uint32_t, SwLight> mLights;
    std::vector<std::uint32_t> mLightIds;
    std::unordered_map<SwLight::Type, std::uint32_t> mStandaloneLightAssetIds;

    std::unordered_map<SwMaterial::Type, std::unordered_map<std::uint32_t, SwBatch>> mBatches;
    std::unordered_map<SwBatchKey, std::uint32_t> mBatchIndicesKeys;

    std::unordered_map<SwPass::Type, SwPass> mPasses;

    SwInput::System mInput;
    SwCull::System mCull;
    SwPick::System mPick;
    SwIBL::System mIBL;
    SwWBOIT::System mWBOIT;
    SwGeometry::System mGeometry;
    SwPostProcess::System mPostProcess;
    SwLighting::System mLighting;
    SwGui::System mGui;

    SwDescriptorSet mSceneMaterialSamplersDescriptorSet;
    SwDescriptorSet mSceneMaterialTexturesDescriptorSet;
    SwDescriptorLayout mSceneMaterialResourcesDescriptorLayout;
    SwAllocatedBuffer mSceneVertexBuffer;
    SwAllocatedBuffer mSceneIndexBuffer;
    SwAllocatedBuffer mSceneMaterialConstantsBuffer;
    SwAllocatedBuffer mSceneNodeTransformsBuffer;
    SwAllocatedBuffer mSceneInstancesBuffer;
    SwAllocatedBuffer mSceneBoundsBuffer;
    SwAllocatedBuffer mSceneRisIndicesBuffer;
    std::array<SwAllocatedBuffer, 2> mSceneVisibilityRisBuffers;
    std::uint32_t mSceneVisibilityRisBufferReadIndex{0};
    SwAllocatedBuffer mSceneLightsBuffer;
    struct PendingRenderCommand {
        SwRenderCommand mRc;
        SwMaterial::Type mMaterialType{SwMaterial::Type::Opaque};
        std::uint32_t mPipelineId{0};
        std::uint32_t mInstanceCount{0};
    };
    std::vector<PendingRenderCommand> mPendingRcs;
    std::vector<SwRenderCommand> mSceneRcs;
    std::vector<SwRenderItem> mSceneRis;
    SwAllocatedBuffer mSceneInitialRcsBuffer;
    SwAllocatedBuffer mSceneEarlyRcsBuffer;
    SwAllocatedBuffer mSceneEarlyRcsCount;
    SwAllocatedBuffer mSceneLateRcsBuffer;
    SwAllocatedBuffer mSceneLateRcsCount;
    SwAllocatedBuffer mSceneRisBuffer;
    SwAllocatedBuffer mSceneBatchesBuffer;

    SwRenderGraph mRenderGraph;

    void initializeMiscPasses();
    void initializeResources();
    void loadStandaloneLightAssets();

    void refreshDependencies();
    void refresh();

    void finalPresentTransition(SwCommandBuffer& commandBuffer);

public:
    static constexpr std::size_t SCENE_INITIAL_VERTEX_BUFFER_SIZE{1ull << 28};
    static constexpr std::size_t SCENE_INITIAL_INDEX_BUFFER_SIZE{1ull << 28};
    static constexpr std::size_t SCENE_INITIAL_NUM_MATERIALS{1 << 8};
    static constexpr std::size_t SCENE_INITIAL_MATERIAL_CONSTANTS_BUFFER_SIZE{SCENE_INITIAL_NUM_MATERIALS * sizeof(SwMaterialConstants)};
    static constexpr std::size_t SCENE_INITIAL_NODE_TRANSFORMS_BUFFER_SIZE{(1 << 12) * sizeof(glm::mat4)};
    static constexpr std::size_t SCENE_INITIAL_INSTANCES_BUFFER_SIZE{(1 << 8) * sizeof(SwInstance::Data)};
    static constexpr std::size_t SCENE_INITIAL_BOUNDS_BUFFER_SIZE{(1 << 12) * sizeof(SwBounds)};
    static constexpr std::size_t SCENE_INITIAL_NUM_RENDER_ITEMS{1 << 12};
    static constexpr std::size_t SCENE_INITIAL_RENDER_ITEMS_INDICES_BUFFER_SIZE{SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(std::uint32_t)};
    static constexpr std::size_t SCENE_INITIAL_NUM_RENDER_COMMANDS{1 << 10};
    static constexpr std::size_t SCENE_INITIAL_RENDER_COMMANDS_BUFFER_SIZE{SCENE_INITIAL_NUM_RENDER_COMMANDS * sizeof(SwRenderCommand)};
    static constexpr std::size_t SCENE_INITIAL_RENDER_COMMANDS_COUNT_BUFFER_SIZE{SCENE_INITIAL_NUM_RENDER_COMMANDS * sizeof(std::uint32_t)};
    static constexpr std::size_t SCENE_INITIAL_RENDER_ITEMS_BUFFER_SIZE{SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(SwRenderItem)};
    static constexpr std::size_t SCENE_INITIAL_LIGHTS_BUFFER_SIZE{(1 << 6) * sizeof(SwLight::Data)};
    static constexpr std::size_t SCENE_INITIAL_NUM_BATCHES{1 << 8};
    static constexpr std::size_t SCENE_INITIAL_BATCHES_COUNT_BUFFER_SIZE{SCENE_INITIAL_NUM_BATCHES * sizeof(std::uint32_t)};
    static constexpr std::size_t SCENE_INITIAL_BATCHES_BUFFER_SIZE{SCENE_INITIAL_NUM_BATCHES * sizeof(SwBatch::Data)};

    Flags mFlags;

    SwScene();

    static void init();

    void initialize();
    void resize();

    void insertPass(SwPass::Type type, std::function<void(vk::CommandBuffer)> callback, bool mustRun = false);

    auto getBatchIt(const std::array<std::optional<SwMaterial::Type>, SwMaterial::NUM_TYPES>& requested) {
        return mBatches | std::views::filter([requested](const auto& pair) { return std::ranges::find(requested, pair.first) != requested.end(); }) |
               std::views::values | std::views::join | std::views::values;
    }

    inline SwCamera& getCamera() { return mCamera; }
    inline SwAsset& getAsset(const std::uint32_t assetId) { return mAssets[assetId]; }
    inline std::unordered_map<std::uint32_t, SwAsset>& getAssets() { return mAssets; }

    std::uint32_t registerInstance(std::uint32_t assetId, SwInstance::Data instanceData);
    inline SwInstance& getInstance(const std::uint32_t instanceId) { return mInstances.at(instanceId); }
    inline std::unordered_map<std::uint32_t, SwInstance>& getInstances() { return mInstances; }

    inline std::unordered_map<std::uint32_t, SwLight>& getLights() { return mLights; }
    inline SwLight& getLight(const std::uint32_t lightId) { return mLights.at(lightId); }
    inline const std::vector<std::uint32_t>& getLightIds() const { return mLightIds; }

    inline SwDescriptorSet& getSceneMaterialSamplersDescriptorSet() { return mSceneMaterialSamplersDescriptorSet; }
    inline SwDescriptorSet& getSceneMaterialTexturesDescriptorSet() { return mSceneMaterialTexturesDescriptorSet; }
    inline SwAllocatedBuffer& getSceneVertexBuffer() { return mSceneVertexBuffer; }
    inline SwAllocatedBuffer& getSceneIndexBuffer() { return mSceneIndexBuffer; }
    inline SwAllocatedBuffer& getSceneMaterialConstantsBuffer() { return mSceneMaterialConstantsBuffer; }
    inline SwAllocatedBuffer& getSceneNodeTransformsBuffer() { return mSceneNodeTransformsBuffer; }
    inline SwAllocatedBuffer& getSceneInstancesBuffer() { return mSceneInstancesBuffer; }
    inline SwAllocatedBuffer& getSceneBoundsBuffer() { return mSceneBoundsBuffer; }
    inline SwAllocatedBuffer& getSceneRisIndicesBuffer() { return mSceneRisIndicesBuffer; }
    inline SwAllocatedBuffer& getSceneLightsBuffer() { return mSceneLightsBuffer; }
    inline void toggleSceneVisibilityRisBuffer() { mSceneVisibilityRisBufferReadIndex = 1 - mSceneVisibilityRisBufferReadIndex; }
    inline SwAllocatedBuffer& getSceneVisibilityRisReadBuffer() { return mSceneVisibilityRisBuffers[mSceneVisibilityRisBufferReadIndex]; }
    inline SwAllocatedBuffer& getSceneVisibilityRisWriteBuffer() { return mSceneVisibilityRisBuffers[1 - mSceneVisibilityRisBufferReadIndex]; }
    inline std::span<SwRenderCommand> getSceneRcs() { return mSceneRcs; }
    inline std::span<SwRenderItem> getSceneRis() { return mSceneRis; }
    inline SwAllocatedBuffer& getSceneInitialRcsBuffer() { return mSceneInitialRcsBuffer; }
    inline SwAllocatedBuffer& getSceneEarlyRcsBuffer() { return mSceneEarlyRcsBuffer; }
    inline SwAllocatedBuffer& getSceneEarlyRcsCount() { return mSceneEarlyRcsCount; }
    inline SwAllocatedBuffer& getSceneLateRcsBuffer() { return mSceneLateRcsBuffer; }
    inline SwAllocatedBuffer& getSceneLateRcsCount() { return mSceneLateRcsCount; }
    inline SwAllocatedBuffer& getSceneRisBuffer() { return mSceneRisBuffer; }
    inline SwAllocatedBuffer& getSceneBatchesBuffer() { return mSceneBatchesBuffer; }

    inline SwInput::System& getInputSystem() { return mInput; }
    inline SwCull::System& getCullSystem() { return mCull; }
    inline SwPick::System& getPickSystem() { return mPick; }
    inline SwIBL::System& getIBLSystem() { return mIBL; }
    inline SwPostProcess::System& getPostProcessSystem() { return mPostProcess; }
    inline SwLighting::System& getLightingSystem() { return mLighting; }
    inline SwRenderGraph& getRenderGraph() { return mRenderGraph; }

    void loadAssets(const std::vector<std::filesystem::path>& files);
    void unloadAssetsAndInstances();
    void markAllAssetsDelete();
    void registerLoadedAsset(std::uint32_t assetId);
    void fillAssetImages();
    void freeAssetImages();
    void fillAssetBuffers();
    void freeAssetBuffers();

    void addInstanceLights(SwAsset& asset, std::uint32_t instanceId);
    void removeInstanceLights(std::uint32_t instanceId);
    void refreshLightIndices();
    void spawnStandaloneLight(SwLight::Type type);

    void recordPendingDraw(SwMaterial& material, const SwRenderCommand& rc, std::uint32_t instanceCount);
    void regenerateRcsAndRis();
    void reloadSceneRcsAndRisBuffers();
    void reloadSceneBatchesBuffer();

    void realignVertexIndexOffset();
    void realignMaterialOffset();
    void realignNodeTransformsOffset();
    void realignBoundsOffset();
    void realignInstancesOffset();
    void realignOffsets();

    void reloadSceneVertexBuffer();
    void reloadSceneIndexBuffer();
    void reloadSceneMaterialConstantsBuffer();
    void reloadSceneNodeTransformsBuffer();
    void reloadSceneBoundsBuffer();
    void reloadSceneInstancesBuffer();
    void reloadSceneLightsBuffer();
    void reloadSceneMaterialResourcesArray();
    void reloadSceneBuffers();

    void resetFlags();

    void startNextFrame();
    void perFrameUpdate();
    void draw();
};