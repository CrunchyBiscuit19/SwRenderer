#pragma once

#include <Data/SwBatch.h>
#include <Data/SwLight.h>
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
static const std::filesystem::path SHADOW_CULL_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwShadowCull.comp.spv"};
static const std::filesystem::path SHADOW_DRAW_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwShadowDraw.vert.spv"};
static constexpr std::string_view SHADOW_DRAW_OPAQUE_TRANSPARENT_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view SHADOW_DRAW_MASKED_ENTRY_POINT{"mainMasked"};
constexpr std::uint32_t NUM_2D_SHADOWS{SwLight::MAX_ACTIVE_LIGHTS};
constexpr std::uint32_t NUM_CUBE_SHADOWS{8};
constexpr std::uint32_t NUM_FRUSTUM_PLANES{6};
constexpr std::uint32_t SHADOW_MAX_RENDER_COMMANDS{1 << 14};
constexpr std::uint32_t SHADOW_RCS_BUFFER_SIZE{SHADOW_MAX_RENDER_COMMANDS * sizeof(SwRenderCommand)};
constexpr std::uint32_t SHADOW_MAP_WIDTH_HEIGHT{1 << 10};
constexpr std::uint32_t SHADOW_CUBE_MAP_WIDTH_HEIGHT{1 << 9};
constexpr vk::Format SHADOW_MAP_FORMAT{vk::Format::eD32Sfloat};

enum class ShadowType : std::uint32_t {
    None = 0,
    TwoD = 1,  
    Cube = 2,  
};

constexpr float SHADOW_DIRECTIONAL_HALF_EXTENT{20.f};
constexpr float SHADOW_DIRECTIONAL_DISTANCE{60.f};
constexpr float SHADOW_DIRECTIONAL_NEAR{0.1f};
constexpr float SHADOW_DIRECTIONAL_FAR{160.f};
constexpr float SHADOW_SPOT_NEAR{0.05f};
constexpr float SHADOW_SPOT_DEFAULT_RANGE{60.f};

struct ShadowCullPC : SwPC<ShadowCullPC> {
    vk::DeviceAddress mShadowRcsBuffer;
    vk::DeviceAddress mShadowRisBuffer;
    vk::DeviceAddress mShadowRisIndicesBuffer;
    vk::DeviceAddress mShadowFrustumsBuffer;
    vk::DeviceAddress mPerFrameBuffer;
    vk::DeviceAddress mSceneBoundsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    std::uint32_t mShadowRisLimit;
    std::uint32_t mLightIndex;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowDrawPC : SwPC<ShadowDrawPC> {
    vk::DeviceAddress mShadowRcsBuffer;
    vk::DeviceAddress mShadowRisIndicesBuffer;
    vk::DeviceAddress mPerFrameBuffer;
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneMaterialConstantsBuffer;
    std::uint32_t mLightIndex;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    static SwDescriptorLayout sShadowConsumeDescriptorLayout;

    static void init();
    static void cleanup();

    std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS> mActiveLightIndices{};
    std::uint32_t mActiveLightCount{0};
    std::array<glm::mat4, SwLight::MAX_ACTIVE_LIGHTS> mLightViewProj{};

    std::array<ShadowType, SwLight::MAX_ACTIVE_LIGHTS> mShadowType{};
    std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS> mShadowIndex{};

    std::array<SwDepthImage2D, NUM_2D_SHADOWS> mShadow2DMaps;
    std::array<SwDepthImageCubemap, NUM_CUBE_SHADOWS> mShadowCubeMaps;
    SwSampler mShadowMapsSampler;
    SwDescriptorSet mShadowMapsDescriptorSet;

    std::vector<SwRenderCommand> mShadowRcs;
    std::array<SwAllocatedBuffer, NUM_2D_SHADOWS + NUM_CUBE_SHADOWS> mShadowRcsBuffer;
    std::array<SwAllocatedBuffer, NUM_2D_SHADOWS + NUM_CUBE_SHADOWS> mShadowRisIndicesBuffer;

    std::array<SwCull::Plane, NUM_2D_SHADOWS * NUM_FRUSTUM_PLANES> mShadowFrustums{};
    SwAllocatedBuffer mShadowFrustumsBuffer;

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
    static void calculateFrustum(const glm::mat4& m, SwCull::Plane* out);

    void resolveLightWorld(const SwLight& light, glm::vec3& outPosition, glm::vec3& outDirection);

    void prepareShadowCullData();

public:
    System(SwScene& scene);

    void refreshDynamicDependencies() override;
    void refreshPushConstants() override;

    void selectActiveLights(const glm::vec3& cameraPos, std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& outIndices, std::uint32_t& outCount);

    void refreshActiveLights(const glm::vec3& cameraPos);

    inline SwDescriptorSet& getShadowMapsDescriptorSet() { return mResources.mShadowMapsDescriptorSet; }
    inline std::uint32_t getActiveLightCount() const { return mResources.mActiveLightCount; }
    inline const std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& getActiveLightIndices() const { return mResources.mActiveLightIndices; }
    inline const std::array<glm::mat4, SwLight::MAX_ACTIVE_LIGHTS>& getLightViewProj() const { return mResources.mLightViewProj; }
    inline const std::array<ShadowType, SwLight::MAX_ACTIVE_LIGHTS>& getShadowTypes() const { return mResources.mShadowType; }
    inline const std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& getShadowIndices() const { return mResources.mShadowIndex; }

    inline Resources& getResources() { return mResources; }
};

}  // namespace SwLighting
