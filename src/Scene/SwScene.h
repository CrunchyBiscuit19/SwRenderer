#pragma once

#include <Data/SwAsset.h>
#include <Data/SwBatch.h>
#include <Data/SwCamera.h>
#include <Resource/SwDescriptor.h>
#include <Scene/SwCull.h>
#include <Scene/SwGeometry.h>
#include <Scene/SwPass.h>
#include <Scene/SwPick.h>
#include <Scene/SwRenderGraph.h>
#include <Scene/SwSkybox.h>
#include <Scene/SwWBOIT.h>

enum class SwPassType {
    ClearScreen,
    CullReset,
    CullDepthPyramid,
    CullWork,
    CullCompact,
    PickDraw,
    PickWork,
    Skybox,
    GeometryOpaque,
    GeometryTransparent,
    WBOITComposite,
    ImGui
};

struct SwSceneFlags {
    bool mAssetLoaded;
    bool mAssetUnloaded;
    bool mInstanceLoaded;
    bool mInstanceUnloaded;
    bool mReloadMainInstancesBuffer;
};

class SwScene {
private:
    static std::filesystem::path CULL_RESET_COMPUTE_SHADER_PATH;
    static std::filesystem::path CULL_WORK_COMPUTE_SHADER_PATH;
    static std::filesystem::path CULL_COMPACT_COMPUTE_SHADER_PATH;
    static std::filesystem::path CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH;
    static constexpr std::uint32_t CULL_MAX_DEPTH_PYRAMID_LEVELS{16};

    static std::filesystem::path PICK_DRAW_VERTEX_SHADER_PATH;
    static std::filesystem::path PICK_DRAW_FRAGMENT_SHADER_PATH;
    static std::filesystem::path PICK_WORK_COMPUTE_SHADER_PATH;

    static std::filesystem::path SKYBOX_VERTEX_SHADER_PATH;
    static std::filesystem::path SKYBOX_FRAGMENT_SHADER_PATH;

    static std::filesystem::path WBOIT_VERTEX_SHADER_PATH;
    static std::filesystem::path WBOIT_FRAGMENT_SHADER_PATH;

    static constexpr std::uint32_t SCENE_VERTEX_BUFFER_SIZE{1 << 30};
    static constexpr std::uint32_t SCENE_INDEX_BUFFER_SIZE{1 << 30};
    static constexpr std::uint32_t SCENE_NUM_MATERIALS{1 << 8};
    static constexpr std::uint32_t SCENE_NUM_NODES{1 << 12};
    static constexpr std::uint32_t SCENE_NUM_INSTANCES{1 << 8};
    static constexpr std::uint32_t SCENE_NUM_BOUNDS{1 << 12};
    static constexpr std::uint32_t SCENE_NUM_RENDER_INSTANCES{1 << 20};

    static SwRendererContext sRendererContext;

    // --- Camera and Assets ---
    SwCamera mCamera;
    std::unordered_map<std::string, SwAsset> mAssets;

    // --- Batches ---
    std::unordered_map<std::uint32_t, SwBatch> mOpaqueBatches;
    std::unordered_map<std::uint32_t, SwBatch> mMaskBatches;
    std::unordered_map<std::uint32_t, SwBatch> mTransparentBatches;

    // --- Passes and Resources ---
    std::unordered_map<SwPassType, SwPass> mPasses;
    SwCull::Resources mCullResources;
    SwPick::Resources mPickResources;
    SwSkybox::Resources mSkyboxResources;
    SwWBOIT::Resources mWBOITResources;
    SwGeometry::Resources mGeometryResources;

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

    void initializeSceneResources();

    void initializeCullResources();
    void onResizeInitializeCullResources();

    void initializePickResources();
    void onResizeInitializePickResources();

    void initializeSkyboxResources();
    void onUpdateInitializeSkyboxResources();

    void initializeWBOITResources();
    void onResizeInitializeWBOITResources();

    void initializeGeometryResources();

public:
    SwSceneFlags mFlags;

    static void init(SwRendererContext rendererContext);

    void initialize();
    void resize();

    void changePickOperation();
    void generatePickFrame();

    inline SwCamera& getCamera() { return mCamera; }

    inline std::unordered_map<std::uint32_t, SwBatch>& getBatches(fastgltf::AlphaMode alphaMode) {
        switch (alphaMode) {
            case fastgltf::AlphaMode::Opaque:
                return mOpaqueBatches;
            case fastgltf::AlphaMode::Mask:
                return mMaskBatches;
            case fastgltf::AlphaMode::Blend:
                return mTransparentBatches;
        }
        std::unreachable();
    }
    
    inline SwAsset& getAsset(const std::string& assetName) { return mAssets[assetName]; }
    void loadAssets(const std::vector<std::filesystem::path>& files);
    void unloadAssets();
    void unloadInstances();
    void markAllAssetsDelete();

    void regenerateRenderItemsInstances();

    void realignVertexIndexOffset();
    void realignMaterialOffset();
    void realignNodeTransformsOffset();
    void realignBoundsOffset();
    void realignInstancesOffset();
    void realignOffsets();

    void reloadMainVertexBuffer();
    void reloadMainIndexBuffer();
    void reloadMainMaterialConstantsBuffer();
    void reloadMainNodeTransformsBuffer();
    void reloadMainBoundsBuffer();
    void reloadMainInstancesBuffer();
    void reloadMainMaterialResourcesArray();
    void reloadMainBuffers();

    void resetFlags();

    void perFrameUpdate();
    void draw();
};

// std::memcpy(mFrustumBuffer.info.pMappedData, planes.data(), FRUSTUM_NUM_PLANES * sizeof(Plane)); // TODO put this inside cull work pass
// TODO define all pass executions and RW dependencies