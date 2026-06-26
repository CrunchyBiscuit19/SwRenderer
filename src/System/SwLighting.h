#pragma once

#include <Data/SwLight.h>
#include <Scene/SwSystem.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>

#include <array>
#include <vector>

class SwInstance;

namespace SwLighting {
static const std::filesystem::path SHADOW_CULL_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwShadowCull.comp.spv"};
static const std::filesystem::path SHADOW_DRAW_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwShadowDraw.vert.spv"};
static constexpr std::string_view SHADOW_DRAW_OPAQUE_TRANSPARENT_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view SHADOW_DRAW_MASKED_ENTRY_POINT{"mainMasked"};
constexpr std::uint32_t NUM_SPOT_SHADOWS{SwLight::MAX_ACTIVE_LIGHTS};
constexpr std::uint32_t NUM_POINT_SHADOWS{8};
constexpr std::uint32_t NUM_DIR_SHADOWS{4};
constexpr std::uint32_t SHADOW_MAP_WIDTH_HEIGHT{1 << 10};
constexpr std::uint32_t SHADOW_CUBE_MAP_WIDTH_HEIGHT{1 << 9};  
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

struct AssetLight {
    SwLight* mLight{nullptr};               
    SwInstance* mInstance{nullptr};         
    std::uint32_t mAssetId{0};              
    std::uint32_t mNodeTransformIndex{0};   
    std::uint32_t mInstanceIndex{0};        
    glm::vec3 mWorldPosition{0.f};          
    glm::vec3 mWorldDirection{0.f};         // light forward (glTF local -Z) in world space
};

struct Resources {
    static SwDescriptorLayout sSpotShadowConsumeDescriptorLayout;
    static SwDescriptorLayout sPointShadowConsumeDescriptorLayout;
    static SwDescriptorLayout sDirShadowConsumeDescriptorLayout;

    static void init();
    static void cleanup();

    std::vector<AssetLight> mAssetLights;

    std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS> mActiveLightIndices{};
    std::uint32_t mActiveLightCount{0};
    std::array<glm::mat4, SwLight::MAX_ACTIVE_LIGHTS> mLightViewProj{};

    std::array<std::uint32_t, NUM_SPOT_SHADOWS> mSpotShadowLightIndices{};
    std::uint32_t mSpotShadowCount{0};
    std::array<std::uint32_t, NUM_POINT_SHADOWS> mPointShadowLightIndices{};
    std::uint32_t mPointShadowCount{0};
    std::array<std::uint32_t, NUM_DIR_SHADOWS> mDirShadowLightIndices{};
    std::uint32_t mDirShadowCount{0};

    std::array<SwDepthImage2D, NUM_SPOT_SHADOWS> mSpotShadowMaps;
    SwSampler mSpotShadowMapsSampler;
    SwDescriptorSet mSpotShadowMapsDescriptorSet;

    std::array<SwDepthImageCubemap, NUM_POINT_SHADOWS> mPointShadowMaps;
    SwSampler mPointShadowMapsSampler;
    SwDescriptorSet mPointShadowMapsDescriptorSet;

    std::array<SwDepthImage2D, NUM_DIR_SHADOWS> mDirShadowMaps;
    SwSampler mDirShadowMapsSampler;
    SwDescriptorSet mDirShadowMapsDescriptorSet;

    std::array<SwAllocatedBuffer, NUM_SPOT_SHADOWS> mSpotLightDrawRisIndicesBuffer;
    std::array<SwAllocatedBuffer, NUM_SPOT_SHADOWS> mSpotLightRcsBuffer;

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

    static glm::mat4 computeLightMatrix(const SwLight::Params& params, const glm::vec3& worldPos, const glm::vec3& worldDir);

public:
    System(SwScene& scene);

    void refreshDynamicDependencies() override;
    void refreshPushConstants() override;

    void selectActiveLights(const glm::vec3& cameraPos, std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& outIndices, std::uint32_t& outCount) const;

    void refreshActiveLights(const glm::vec3& cameraPos);

    std::vector<SwLight::Data> collectLightData() const;

    inline SwDescriptorSet& getSpotShadowMapsDescriptorSet() { return mResources.mSpotShadowMapsDescriptorSet; }
    inline SwDescriptorSet& getPointShadowMapsDescriptorSet() { return mResources.mPointShadowMapsDescriptorSet; }
    inline SwDescriptorSet& getDirShadowMapsDescriptorSet() { return mResources.mDirShadowMapsDescriptorSet; }
    inline std::uint32_t getActiveLightCount() const { return mResources.mActiveLightCount; }
    inline const std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& getActiveLightIndices() const { return mResources.mActiveLightIndices; }
    inline const std::array<glm::mat4, SwLight::MAX_ACTIVE_LIGHTS>& getLightViewProj() const { return mResources.mLightViewProj; }

    inline Resources& getResources() { return mResources; }
    inline std::vector<AssetLight>& getAssetLights() { return mResources.mAssetLights; }
};

}  // namespace SwLighting
