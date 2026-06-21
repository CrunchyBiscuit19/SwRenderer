#pragma once

#include <Data/SwLight.h>
#include <Scene/SwSystem.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>

#include <vector>

namespace SwLighting {
static const std::filesystem::path SHADOW_CULL_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwShadowCull.comp.spv"};
static const std::filesystem::path SHADOW_DRAW_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwShadowDraw.vert.spv"};
constexpr std::uint32_t NUM_LIGHT_CAST_SHADOWS{1 << 4};
constexpr std::uint32_t SHADOW_MAP_WIDTH_HEIGHT{1 << 10};
constexpr vk::Format SHADOW_MAP_FORMAT{vk::Format::eD32Sfloat};

struct ShadowCullPC : SwPC<ShadowCullPC> {
    vk::DeviceAddress mLightRcsBuffer;
    vk::DeviceAddress mRisBuffer;
    vk::DeviceAddress mRisCount;
    vk::DeviceAddress mLightDrawRisIndicesBuffer;
    vk::DeviceAddress mFrustumBuffer;
    vk::DeviceAddress mPerFrameBuffer;
    vk::DeviceAddress mSceneBoundsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowDrawPC : SwPC<ShadowDrawPC> {
    vk::DeviceAddress mLightDrawRisIndicesBuffer;
    vk::DeviceAddress mLightRcsBuffer;
    vk::DeviceAddress mPerFrameBuffer;
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    SwSunlight mSunlight;                     
    std::vector<SwLight::Data> mAssetLights;  
    std::vector<SwLight> mGlobalLights;       

    std::array<SwDepthImage2D, NUM_LIGHT_CAST_SHADOWS> mShadowMaps;
    SwSampler mShadowMapsSampler;
    
    SwDescriptorLayout mShadowMapsDescriptorLayout;
    SwDescriptorSet mShadowMapsDescriptorSet;

    std::array<SwAllocatedBuffer, NUM_LIGHT_CAST_SHADOWS> mLightDrawRisIndicesBuffer;
    std::array<SwAllocatedBuffer, NUM_LIGHT_CAST_SHADOWS> mLightRcsBuffer;

    ShadowCullPC mShadowCullPc;
    SwPipelineLayout mShadowCullPipelineLayout;
    SwComputePipelineBundle mShadowCullPipelineBundle;
    
    ShadowDrawPC mShadowDrawPc;
    SwPipelineLayout mShadowDrawPipelineLayout;
    SwGraphicsPipelineBundle mShadowDrawPipelineBundle;
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

    inline Resources& getResources() { return mResources; }
    inline SwSunlight& getSunlight() { return mResources.mSunlight; }
    inline std::vector<SwLight::Data>& getAssetLights() { return mResources.mAssetLights; }
    inline std::vector<SwLight>& getGlobalLights() { return mResources.mGlobalLights; }
};

}  // namespace SwLighting
