#pragma once

#include <Data/SwBatch.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Resource/SwSampler.h>
#include <Scene/SwScene.h>
#include <Scene/SwSystem.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

// TODO This entire fucking system needs a rework

class SwInstance;

namespace SwLighting {
static const std::filesystem::path LIGHTING_SHADERS_DIR{std::filesystem::path(SHADERS_DIR) / "Lighting"};
static const std::filesystem::path SHADOW_RESET_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowReset.comp.spv"};
static const std::filesystem::path BUILD_CLUSTERS_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingBuildClusters.comp.spv"};
static const std::filesystem::path MARK_ACTIVE_CLUSTERS_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingMarkActiveClusters.comp.spv"};
static const std::filesystem::path LIGHTS_CULL_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingLightsCull.comp.spv"};
static const std::filesystem::path SHADOW_CULL_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowCull.comp.spv"};
static const std::filesystem::path SHADOW_DRAW_VERTEX_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowDraw.vert.spv"};
static constexpr std::string_view SHADOW_DRAW_OPAQUE_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view SHADOW_DRAW_MASKED_ENTRY_POINT{"mainMasked"};

static constexpr std::uint32_t MAX_NUM_SHADOW_CASTERS{16};
constexpr std::uint64_t SHADOW_INITIAL_RENDER_COMMANDS_BUFFER_SIZE{(1 << 10) * sizeof(SwRenderCommand)};
constexpr std::uint32_t SHADOW_MAP_WIDTH_HEIGHT{1 << 10};
constexpr std::uint32_t SHADOW_CUBE_MAP_WIDTH_HEIGHT{1 << 9};
constexpr vk::Format SHADOW_MAP_FORMAT{vk::Format::eD32Sfloat};

constexpr float SHADOW_DIRECTIONAL_HALF_EXTENT{20.f};
constexpr float SHADOW_DIRECTIONAL_DISTANCE{60.f};
constexpr float SHADOW_DIRECTIONAL_NEAR{0.1f};
constexpr float SHADOW_DIRECTIONAL_FAR{160.f};
constexpr float SHADOW_SPOT_NEAR{0.05f};
constexpr float SHADOW_SPOT_DEFAULT_RANGE{60.f};

constexpr std::uint32_t INITIAL_ACTIVE_LIGHTS_BUFFER_SIZE{1 << 10};

// LightingShadowReset, LightingBuildClusters, LightingMarkActiveClusters, LightingLightsCull, LightingShadowCull, LightingShadowDraw,

struct ResetPC : SwPC<ResetPC> {
    vk::DeviceAddress mShadowRcsBuffer;
    std::uint32_t mShadowRcsLimit;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClusterBuildPC : SwPC<ClusterBuildPC> {
    vk::DeviceAddress mClustersBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClusterMarkActivePC : SwPC<ClusterMarkActivePC> {
    vk::DeviceAddress mActiveClustersIndicesBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClusterCullLightsPC : SwPC<ClusterCullLightsPC> {
    vk::DeviceAddress mFrameBuffer;
    vk::DeviceAddress mSceneLightsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mVisibleLightsBuffer;
    vk::DeviceAddress mActiveClustersIndicesBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct CullPC : SwPC<CullPC> {
    vk::DeviceAddress mShadowRcsBuffer;
    vk::DeviceAddress mShadowRisBuffer;
    vk::DeviceAddress mShadowRisIndicesBuffer;
    vk::DeviceAddress mSceneLightsBuffer;
    vk::DeviceAddress mSceneBoundsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mVisibleLightsBuffer;
    std::uint32_t mShadowRisLimit;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mShadowRcsBuffer;
    vk::DeviceAddress mShadowRisIndicesBuffer;
    vk::DeviceAddress mSceneLightsBuffer;
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneMaterialConstantsBuffer;
    vk::DeviceAddress mVisibleLightsBuffer;
    std::uint32_t mLightIndex;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    static SwDescriptorLayout sShadowConsumeDescriptorLayout;

    static void init();
    static void cleanup();

    SwAllocatedBuffer mVisibleLightsBuffer;

    std::array<SwDepthImage2D, MAX_NUM_SHADOW_CASTERS> mShadow2DMaps;
    std::array<SwDepthImageCubemap, MAX_NUM_SHADOW_CASTERS> mShadowCubeMaps;
    SwSampler mShadowMapsSampler;
    SwDescriptorSet mShadowMapsDescriptorSet;

    std::vector<SwRenderCommand> mInitialShadowRcs;
    std::array<SwAllocatedBuffer, MAX_NUM_SHADOW_CASTERS> mShadowRcsBuffer;
    std::array<SwAllocatedBuffer, MAX_NUM_SHADOW_CASTERS> mShadowRisBuffer;
    std::array<SwAllocatedBuffer, MAX_NUM_SHADOW_CASTERS> mShadowRisIndicesBuffer;

    ResetPC mShadowResetPc;
    SwPipelineLayout mShadowResetPipelineLayout;
    SwComputePipelineBundle mShadowResetPipelineBundle;

    ClusterBuildPC mBuildClustersPc;
    SwPipelineLayout mBuildClustersPipelineLayout;
    SwComputePipelineBundle mBuildClustersPipelineBundle;

    ClusterMarkActivePC mMarkActiveClustersPc;
    SwPipelineLayout mMarkActiveClustersPipelineLayout;
    SwComputePipelineBundle mMarkActiveClustersPipelineBundle;

    ClusterCullLightsPC mLightsCullPc;
    SwPipelineLayout mLightsCullPipelineLayout;
    SwComputePipelineBundle mLightsCullPipelineBundle;

    CullPC mShadowCullPc;
    SwPipelineLayout mShadowCullPipelineLayout;
    SwComputePipelineBundle mShadowCullPipelineBundle;

    DrawPC mShadowDrawPc;
    SwPipelineLayout mShadowDrawPipelineLayout;
    SwGraphicsPipelineBundle mShadowDrawOpaquePipelineBundle;
    SwGraphicsPipelineBundle mShadowDrawMaskedPipelineBundle;
};

class System : public SwSystem {
private:
    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;
    void refreshDependencies() override;
    void refreshPushConstants() override;

public:
    System(SwScene& scene);

    void regenerateShadowRcs();

    inline SwDescriptorSet& getShadowMapsDescriptorSet() { return mResources.mShadowMapsDescriptorSet; }
    inline SwAllocatedBuffer& getVisibleLightsBuffer() { return mResources.mVisibleLightsBuffer; }

    inline Resources& getResources() { return mResources; }
};

}  // namespace SwLighting
