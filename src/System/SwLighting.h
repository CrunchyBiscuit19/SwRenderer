#pragma once

#include <Data/SwBatch.h>
#include <Data/SwLight.h>
#include <Data/SwCamera.h>
#include <Scene/SwSystem.h>
#include <System/SwCull.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>

#include <array>
#include <unordered_map>
#include <vector>

class SwInstance;

namespace SwLighting {
static const std::filesystem::path LIGHTING_SHADERS_DIR{std::filesystem::path(SHADERS_DIR) / "Lighting"};
static const std::filesystem::path ACTIVE_LIGHTS_SELECTION_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwActiveLightsSelection.comp.spv"};
static const std::filesystem::path SHADOW_RESET_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwShadowReset.comp.spv"};
static const std::filesystem::path SHADOW_CULL_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwShadowCull.comp.spv"};
static const std::filesystem::path SHADOW_DRAW_VERTEX_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwShadowDraw.vert.spv"};
static constexpr std::string_view SHADOW_DRAW_OPAQUE_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view SHADOW_DRAW_MASKED_ENTRY_POINT{"mainMasked"};

static constexpr std::uint32_t MAX_ACTIVE_LIGHTS{16};
constexpr std::uint32_t SHADOW_INITIAL_RENDER_COMMANDS{1 << 10};
constexpr std::uint32_t SHADOW_MAP_WIDTH_HEIGHT{1 << 10};
constexpr std::uint32_t SHADOW_CUBE_MAP_WIDTH_HEIGHT{1 << 9};
constexpr vk::Format SHADOW_MAP_FORMAT{vk::Format::eD32Sfloat};

constexpr float SHADOW_DIRECTIONAL_HALF_EXTENT{20.f};
constexpr float SHADOW_DIRECTIONAL_DISTANCE{60.f};
constexpr float SHADOW_DIRECTIONAL_NEAR{0.1f};
constexpr float SHADOW_DIRECTIONAL_FAR{160.f};
constexpr float SHADOW_SPOT_NEAR{0.05f};
constexpr float SHADOW_SPOT_DEFAULT_RANGE{60.f};

struct ActiveLights {
    std::uint32_t mCount{0};
    std::array<std::uint32_t, MAX_ACTIVE_LIGHTS> mActiveLightIndices{};
    std::array<SwFrustum, MAX_ACTIVE_LIGHTS> mActiveLightFrustums{};
};

struct ActiveLightsSelectionPC : SwPC<ActiveLightsSelectionPC> {
    vk::DeviceAddress mCameraBuffer;
    vk::DeviceAddress mSceneLightsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mActiveLightsBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowResetPC : SwPC<ShadowResetPC> {
    vk::DeviceAddress mShadowDrawRcsBuffer;
    std::uint32_t mShadowRcsLimit;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowCullPC : SwPC<ShadowCullPC> {
    vk::DeviceAddress mShadowDrawRcsBuffer;
    vk::DeviceAddress mShadowDrawRisBuffer;
    vk::DeviceAddress mShadowDrawRisIndicesBuffer;
    vk::DeviceAddress mSceneLightsBuffer;
    vk::DeviceAddress mSceneBoundsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mActiveLightsBuffer; 
    std::uint32_t mShadowRisLimit;
    std::uint32_t mLightIndex;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowDrawPC : SwPC<ShadowDrawPC> {
    vk::DeviceAddress mShadowDrawRcsBuffer;
    vk::DeviceAddress mShadowDrawRisIndicesBuffer;
    vk::DeviceAddress mSceneLightsBuffer;
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneMaterialConstantsBuffer;
    vk::DeviceAddress mActiveLightsBuffer;
    std::uint32_t mLightIndex;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    static SwDescriptorLayout sShadowConsumeDescriptorLayout;
    static SwStagingBuffer sShadowRcsStaging;

    static void init();
    static void cleanup();

    SwAllocatedBuffer mActiveLightsBuffer; 

    std::array<SwDepthImage2D, MAX_ACTIVE_LIGHTS> mShadow2DMaps;
    std::array<SwDepthImageCubemap, MAX_ACTIVE_LIGHTS> mShadowCubeMaps;
    SwSampler mShadowMapsSampler;
    SwDescriptorSet mShadowMapsDescriptorSet;

    std::vector<SwRenderCommand> mShadowRcs;
    std::array<SwAllocatedBuffer, MAX_ACTIVE_LIGHTS> mShadowDrawRcsBuffer; 
    std::array<SwAllocatedBuffer, MAX_ACTIVE_LIGHTS> mShadowDrawRisBuffer; 
    std::array<SwAllocatedBuffer, MAX_ACTIVE_LIGHTS> mShadowDrawRisIndicesBuffer; 

    ActiveLightsSelectionPC mActiveLightsSelectionPc;
    SwPipelineLayout mActiveLightsSelectionPipelineLayout;
    SwComputePipelineBundle mActiveLightsSelectionPipelineBundle;

    ShadowResetPC mShadowResetPc;
    SwPipelineLayout mShadowResetPipelineLayout;
    SwComputePipelineBundle mShadowResetPipelineBundle;

    ShadowCullPC mShadowCullPc;
    SwPipelineLayout mShadowCullPipelineLayout;
    SwComputePipelineBundle mShadowCullPipelineBundle;
    
    ShadowDrawPC mShadowDrawPc;
    SwPipelineLayout mShadowDrawPipelineLayout;
    SwGraphicsPipelineBundle mShadowDrawOpaquePipelineBundle;
    SwGraphicsPipelineBundle mShadowDrawMaskedPipelineBundle;
};

class System : public SwSystem {
private:
    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;

public:
    System(SwScene& scene);

    void refreshDynamicDependencies() override;
    void refreshPushConstants() override;

    void regenerateShadowRcs();

    inline SwDescriptorSet& getShadowMapsDescriptorSet() { return mResources.mShadowMapsDescriptorSet; }
    inline SwAllocatedBuffer& getActiveLightsBuffer() { return mResources.mActiveLightsBuffer; }

    inline Resources& getResources() { return mResources; }
};

}  // namespace SwLighting
