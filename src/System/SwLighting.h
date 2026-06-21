#pragma once

#include <Data/SwLight.h>
#include <Scene/SwSystem.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>

#include <array>
#include <vector>

namespace SwLighting {
static const std::filesystem::path SHADOW_CULL_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwShadowCull.comp.spv"};
static const std::filesystem::path SHADOW_DRAW_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwShadowDraw.vert.spv"};
static constexpr std::string_view SHADOW_DRAW_OPAQUE_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view SHADOW_DRAW_MASKED_ENTRY_POINT{"mainMasked"};
constexpr std::uint32_t NUM_LIGHT_CAST_SHADOWS{SwLight::MAX_ACTIVE_LIGHTS};
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
    vk::DeviceAddress mSceneMaterialConstantsBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    SwSunlight mSunlight;
    std::vector<SwLight::Data> mAssetLights;
    std::vector<glm::vec3> mLightWorldPositions;  // parallel to mAssetLights, cached for per-frame brightness scoring
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

    // Pick the up-to-MAX_ACTIVE_LIGHTS punctual lights that are brightest at cameraPos (intensity x attenuation),
    // writing their scene-light-buffer indices into outIndices and the count into outCount. Directional lights have
    // no attenuation and are always selected. Spot-cone falloff is intentionally omitted from the score.
    void selectActiveLights(const glm::vec3& cameraPos, std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& outIndices, std::uint32_t& outCount) const;

    inline Resources& getResources() { return mResources; }
    inline SwSunlight& getSunlight() { return mResources.mSunlight; }
    inline std::vector<SwLight::Data>& getAssetLights() { return mResources.mAssetLights; }
    inline std::vector<glm::vec3>& getLightWorldPositions() { return mResources.mLightWorldPositions; }
    inline std::vector<SwLight>& getGlobalLights() { return mResources.mGlobalLights; }
};

}  // namespace SwLighting
