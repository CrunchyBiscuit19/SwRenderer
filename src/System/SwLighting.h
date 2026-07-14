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

class SwInstance;

namespace SwLighting {
static const std::filesystem::path LIGHTING_SHADERS_DIR{std::filesystem::path(SHADERS_DIR) / "Lighting"};
static const std::filesystem::path LIGHTING_SHADOWS_RESET_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowReset.comp.spv"};
static const std::filesystem::path LIGHTING_BUILD_CLUSTERS_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingBuildClusters.comp.spv"};
static const std::filesystem::path LIGHTING_MARK_ACTIVE_CLUSTERS_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingMarkActiveClusters.comp.spv"};
static const std::filesystem::path LIGHTING_LIGHTS_CULL_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingLightsCull.comp.spv"};
static const std::filesystem::path LIGHTING_SHADOWS_CULL_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowCull.comp.spv"};
static const std::filesystem::path LIGHTING_SHADOWS_DRAW_VERTEX_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowDraw.vert.spv"};
static constexpr std::string_view LIGHTING_SHADOWS_DRAW_OPAQUE_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view LIGHTING_SHADOWS_DRAW_MASKED_ENTRY_POINT{"mainMasked"};

static constexpr std::uint32_t MAX_NUM_SHADOW_CASTERS{16};
constexpr std::uint64_t LIGHTING_INITIAL_SHADOWS_RENDER_COMMANDS_BUFFER_SIZE{(1 << 10) * sizeof(SwRenderCommand)};
constexpr std::uint32_t LIGHTING_SHADOWS_2D_MAP_WIDTH_HEIGHT{1 << 10};
constexpr std::uint32_t LIGHTING_SHADOWS_CUBEMAP_WIDTH_HEIGHT{1 << 9};
constexpr vk::Format LIGHTING_SHADOWS_MAP_FORMAT{vk::Format::eD32Sfloat};

struct LightsInfo {
    std::uint32_t mLitCount{0};
    vk::DeviceAddress mLitIndices{0};
    std::uint32_t mShadowCastCount{0};
    vk::DeviceAddress mShadowCastIndices{0};
};

struct ClustersBuildPC : SwPC<ClustersBuildPC> {
    vk::DeviceAddress mClustersBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClustersMarkActivePC : SwPC<ClustersMarkActivePC> {
    vk::DeviceAddress mClustersActiveIndicesBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClustersCullPC : SwPC<ClustersCullPC> {
    vk::DeviceAddress mFrameBuffer{0};
    vk::DeviceAddress mSceneLightsBuffer{0};
    vk::DeviceAddress mSceneLightsInfoBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    vk::DeviceAddress mClustersActiveIndicesBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowsResetPC : SwPC<ShadowsResetPC> {
    vk::DeviceAddress mSceneShadowsRcsBuffer{0};
    std::uint32_t mSceneShadowsRcsLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowsCullPC : SwPC<ShadowsCullPC> {
    vk::DeviceAddress mSceneShadowsRcsBuffer{0};
    vk::DeviceAddress mSceneShadowsRisBuffer{0};
    vk::DeviceAddress mSceneShadowsRisIndicesBuffer{0};
    vk::DeviceAddress mSceneLightsBuffer{0};
    vk::DeviceAddress mSceneLightsInfoBuffer{0};
    vk::DeviceAddress mSceneBoundsBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    std::uint32_t mSceneShadowsRisLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowDrawPC : SwPC<ShadowDrawPC> {  // MRTs then select the light's shadow map
    vk::DeviceAddress mSceneShadowsRcsBuffer{0};
    vk::DeviceAddress mSceneShadowsRisIndicesBuffer{0};
    vk::DeviceAddress mSceneLightsBuffer{0};
    vk::DeviceAddress mSceneLightsInfoBuffer{0};
    vk::DeviceAddress mSceneVertexBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    vk::DeviceAddress mSceneMaterialConstantsBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    static SwDescriptorLayout sShadowsConsumeDescriptorLayout;

    static void init();
    static void cleanup();

    std::array<SwDepthImage2D, MAX_NUM_SHADOW_CASTERS> mShadows2DMaps;
    std::array<SwDepthImageCubemap, MAX_NUM_SHADOW_CASTERS> mShadowsCubeMaps;
    SwSampler mShadowsMapsSampler;
    SwDescriptorSet mShadowsMapsDescriptorSet;

    ClustersBuildPC mClustersBuildPc;
    SwPipelineLayout mClustersBuildPipelineLayout;
    SwComputePipelineBundle mClustersBuildPipelineBundle;

    ClustersMarkActivePC mClustersMarkActivePc;
    SwPipelineLayout mClustersMarkActivePipelineLayout;
    SwComputePipelineBundle mClustersMarkActivePipelineBundle;

    ClustersCullPC mClustersCullPc;
    SwPipelineLayout mClustersCullPipelineLayout;
    SwComputePipelineBundle mClustersCullPipelineBundle;

    ShadowsResetPC mShadowsResetPc;
    SwPipelineLayout mShadowsResetPipelineLayout;
    SwComputePipelineBundle mShadowsResetPipelineBundle;

    ShadowsCullPC mShadowsCullPc;
    SwPipelineLayout mShadowsCullPipelineLayout;
    SwComputePipelineBundle mShadowsCullPipelineBundle;

    ShadowDrawPC mShadowsDrawPc;
    SwPipelineLayout mShadowsDrawPipelineLayout;
    SwGraphicsPipelineBundle mShadowsDrawOpaquePipelineBundle;
    SwGraphicsPipelineBundle mShadowsDrawMaskedPipelineBundle;
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

    void regenerateShadowsRcs();

    inline SwDescriptorSet& getShadowsMapsDescriptorSet() { return mResources.mShadowsMapsDescriptorSet; }

    inline Resources& getResources() { return mResources; }
};

}  // namespace SwLighting
