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
static constexpr std::string_view SHADOW_DRAW_OPAQUE_TRANSPARENT_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view SHADOW_DRAW_MASKED_ENTRY_POINT{"mainMasked"};
constexpr std::uint32_t NUM_LIGHT_CAST_SHADOWS{SwLight::MAX_ACTIVE_LIGHTS};
constexpr std::uint32_t SHADOW_MAP_WIDTH_HEIGHT{1 << 10};
constexpr vk::Format SHADOW_MAP_FORMAT{vk::Format::eD32Sfloat};

constexpr float SHADOW_DIRECTIONAL_HALF_EXTENT{20.f};
constexpr float SHADOW_DIRECTIONAL_DISTANCE{60.f};
constexpr float SHADOW_DIRECTIONAL_NEAR{0.1f};
constexpr float SHADOW_DIRECTIONAL_FAR{160.f};
constexpr float SHADOW_SPOT_NEAR{0.05f};
constexpr float SHADOW_SPOT_DEFAULT_RANGE{60.f};

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
    std::uint32_t mLightIndex;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    // Set-2 layout the geometry/transparent fragment shaders bind to sample the spotlight shadow maps. Static so
    // SwMaterial can bake it into the geometry pipeline layout, mirroring SwIBL::Resources::sConsumeDescriptorLayout.
    static SwDescriptorLayout sShadowConsumeDescriptorLayout;

    static void init();
    static void cleanup();

    SwSunlight mSunlight;
    std::vector<SwLight::Data> mAssetLights;
    std::vector<glm::vec3> mLightWorldPositions;   // parallel to mAssetLights, cached for per-frame brightness scoring
    std::vector<glm::vec3> mLightWorldDirections;  // parallel to mAssetLights, light forward (glTF local -Z) in world space
    std::vector<SwLight> mGlobalLights;

    // Per-frame active-light selection, cached on the CPU so the shadow draw pass can iterate it. Mirrors what
    // gets uploaded into the per-frame buffer each frame.
    std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS> mActiveLightIndices{};
    std::uint32_t mActiveLightCount{0};
    std::array<glm::mat4, SwLight::MAX_ACTIVE_LIGHTS> mLightViewProj{};

    std::array<SwDepthImage2D, NUM_LIGHT_CAST_SHADOWS> mShadowMaps;
    SwSampler mShadowMapsSampler;

    SwDescriptorSet mShadowMapsDescriptorSet;

    std::array<SwAllocatedBuffer, NUM_LIGHT_CAST_SHADOWS> mLightDrawRisIndicesBuffer;
    std::array<SwAllocatedBuffer, NUM_LIGHT_CAST_SHADOWS> mLightRcsBuffer;

    ShadowCullPC mShadowCullPc;
    SwPipelineLayout mShadowCullPipelineLayout;
    SwComputePipelineBundle mShadowCullPipelineBundle;
    
    ShadowDrawPC mShadowDrawPc;
    SwPipelineLayout mShadowDrawPipelineLayout;
    SwGraphicsPipelineBundle mShadowDrawOpaqueTransparentPipelineBundle;
    SwGraphicsPipelineBundle mShadowDrawMaskedPipelineBundle;
};

class System : public SwSystem {
private:
    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;

    static glm::mat4 computeLightMatrix(const SwLight::Data& light, const glm::vec3& worldPos, const glm::vec3& worldDir);

public:
    System(SwScene& scene);

    void refreshDynamicDependencies() override;
    void refreshPushConstants() override;

    // Pick the up-to-MAX_ACTIVE_LIGHTS punctual lights that are brightest at cameraPos (intensity x attenuation),
    // writing their scene-light-buffer indices into outIndices and the count into outCount. Directional lights have
    // no attenuation and are always selected. Spot-cone falloff is intentionally omitted from the score.
    void selectActiveLights(const glm::vec3& cameraPos, std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& outIndices, std::uint32_t& outCount) const;

    void refreshActiveLights(const glm::vec3& cameraPos);

    inline SwDescriptorSet& getShadowMapsDescriptorSet() { return mResources.mShadowMapsDescriptorSet; }
    inline std::uint32_t getActiveLightCount() const { return mResources.mActiveLightCount; }
    inline const std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& getActiveLightIndices() const { return mResources.mActiveLightIndices; }
    inline const std::array<glm::mat4, SwLight::MAX_ACTIVE_LIGHTS>& getLightViewProj() const { return mResources.mLightViewProj; }

    inline Resources& getResources() { return mResources; }
    inline SwSunlight& getSunlight() { return mResources.mSunlight; }
    inline std::vector<SwLight::Data>& getAssetLights() { return mResources.mAssetLights; }
    inline std::vector<glm::vec3>& getLightWorldPositions() { return mResources.mLightWorldPositions; }
    inline std::vector<glm::vec3>& getLightWorldDirections() { return mResources.mLightWorldDirections; }
    inline std::vector<SwLight>& getGlobalLights() { return mResources.mGlobalLights; }
};

}  // namespace SwLighting
