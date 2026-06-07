#pragma once

#include <Data/SwAsset.h>
#include <Data/SwBatch.h>
#include <Data/SwCamera.h>
#include <Gui/SwGui.h>
#include <Resource/SwDescriptor.h>
#include <Scene/System/SwCull.h>
#include <Scene/System/SwSkybox.h>
#include <Scene/SwPass.h>
#include <Scene/System/SwPick.h>
#include <Scene/System/SwGeometry.h>
#include <Scene/SwRenderGraph.h>
#include <Scene/System/SwWBOIT.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <ranges>
#include <unordered_set>

class SwScene {
public:
    struct Flags {
        bool mAssetLoaded;
        bool mAssetUnloaded;
        bool mInstanceLoaded;
        bool mInstanceUnloaded;
        bool mReloadMainInstancesBuffer;
    };

private:
    friend class SwCull::System;
    friend class SwPick::System;
    friend class SwSkybox::System;
    friend class SwWBOIT::System;
    friend class SwGeometry::System;
    friend class SwGui;


    SwCamera mCamera;

    std::unordered_map<std::uint32_t, SwAsset> mAssets;
    std::unordered_set<std::string> mAlreadyLoadedAssets;

    std::unordered_map<SwMaterial::Type, std::unordered_map<std::uint32_t, SwBatch>> mBatchTypes;

    std::unordered_map<SwPass::Type, SwPass> mPasses;

    SwCull::System mCull;
    SwPick::System mPick;
    SwSkybox::System mSkybox;
    SwWBOIT::System mWBOIT;
    SwGeometry::System mGeometry;
    SwGui mGui;

    SwDescriptorSet mSceneMaterialResourcesDescriptorSet;
    SwDescriptorLayout mSceneMaterialResourcesDescriptorLayout;
    SwAllocatedBuffer mSceneVertexBuffer;
    SwAllocatedBuffer mSceneIndexBuffer;
    SwAllocatedBuffer mSceneMaterialConstantsBuffer;
    SwAllocatedBuffer mSceneNodeTransformsBuffer;
    SwAllocatedBuffer mSceneInstancesBuffer;
    SwAllocatedBuffer mSceneBoundsBuffer;
    SwAllocatedBuffer mSceneVisibleRInstsIndicesBuffer;

    SwRenderGraph mRenderGraph;

    void initializeMiscPasses();
    void initializeResources();

    void refreshDynamicDependencies();
    void refresh();

    void finalPresentTransition(SwCommandBuffer& commandBuffer);

public:
    static constexpr std::uint32_t SCENE_INITIAL_VERTEX_BUFFER_SIZE{1 << 28};
    static constexpr std::uint32_t SCENE_INITIAL_INDEX_BUFFER_SIZE{1 << 28};
    static constexpr std::uint32_t SCENE_INITIAL_NUM_MATERIALS{1 << 8};
    static constexpr std::uint32_t SCENE_INITIAL_NUM_NODES{1 << 12};
    static constexpr std::uint32_t SCENE_INITIAL_NUM_INSTANCES{1 << 8};
    static constexpr std::uint32_t SCENE_INITIAL_NUM_BOUNDS{1 << 12};
    static constexpr std::uint32_t SCENE_INITIAL_NUM_RENDER_INSTANCES{1 << 18};

    Flags mFlags;

    SwScene();

    static void init();

    void initialize();
    void resize();

    void insertPass(SwPass::Type type, SwDependency deps, std::function<void(vk::CommandBuffer)> callback, bool mustRun = false);

    template <std::same_as<SwMaterial::Type>... Types>
    auto getBatchIt(Types... types) {
        std::array<SwMaterial::Type, sizeof...(Types)> requested{types...};
        return mBatchTypes | std::views::filter([requested](const auto& pair) { return std::ranges::find(requested, pair.first) != requested.end(); }) |
               std::views::values | std::views::join | std::views::values;
    }
    inline std::unordered_map<std::uint32_t, SwBatch>& getBatchMap(SwMaterial::Type type) { return mBatchTypes[type]; }
    inline SwCamera& getCamera() { return mCamera; }
    inline SwAsset& getAsset(const std::uint32_t assetId) { return mAssets[assetId]; }
    inline std::unordered_map<std::uint32_t, SwAsset>& getAssets() { return mAssets; }
    inline SwDescriptorSet& getSceneMaterialResourcesDescriptorSet() { return mSceneMaterialResourcesDescriptorSet; }
    inline SwAllocatedBuffer& getSceneVertexBuffer() { return mSceneVertexBuffer; }
    inline SwAllocatedBuffer& getSceneIndexBuffer() { return mSceneIndexBuffer; }
    inline SwAllocatedBuffer& getSceneMaterialConstantsBuffer() { return mSceneMaterialConstantsBuffer; }
    inline SwAllocatedBuffer& getSceneNodeTransformsBuffer() { return mSceneNodeTransformsBuffer; }
    inline SwAllocatedBuffer& getSceneInstancesBuffer() { return mSceneInstancesBuffer; }
    inline SwAllocatedBuffer& getSceneBoundsBuffer() { return mSceneBoundsBuffer; }
    inline SwAllocatedBuffer& getSceneVisibleRInstsIndicesBuffer() { return mSceneVisibleRInstsIndicesBuffer; }
    inline SwCull::System& getCullSystem() { return mCull; }
    inline SwPick::System& getPickSystem() { return mPick; }
    inline SwSkybox::System& getSkyboxSystem() { return mSkybox; }

    void loadAssets(const std::vector<std::filesystem::path>& files);
    void unloadAssets();
    void unloadInstances();
    void markAllAssetsDelete();

    void regenerateRItemsAndRInsts();

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
    void reloadSceneMaterialResourcesArray();
    void reloadSceneBuffers();

    void resetFlags();

    void perFrameUpdate();
    void draw();
};