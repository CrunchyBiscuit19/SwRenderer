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
static const std::filesystem::path LIGHTING_SHADOW_RESET_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowReset.comp.spv"};
static const std::filesystem::path LIGHTING_BUILD_CLUSTERS_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingBuildClusters.comp.spv"};
static const std::filesystem::path LIGHTING_MARK_ACTIVE_CLUSTERS_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingMarkActiveClusters.comp.spv"};
static const std::filesystem::path LIGHTING_LIGHTS_CULL_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingLightsCull.comp.spv"};
static const std::filesystem::path LIGHTING_SHADOW_CULL_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowCull.comp.spv"};
static const std::filesystem::path LIGHTING_SHADOW_DRAW_VERTEX_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowDraw.vert.spv"};
static constexpr std::string_view LIGHTING_SHADOW_DRAW_OPAQUE_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view LIGHTING_SHADOW_DRAW_MASKED_ENTRY_POINT{"mainMasked"};

static constexpr std::uint32_t MAX_NUM_SHADOW_CASTERS{16};
constexpr std::uint64_t LIGHTING_INITIAL_SHADOW_RENDER_COMMANDS_BUFFER_SIZE{(1 << 10) * sizeof(SwRenderCommand)};
constexpr std::uint32_t LIGHTING_SHADOW_2D_MAP_WIDTH_HEIGHT{1 << 10};
constexpr std::uint32_t LIGHTING_SHADOW_CUBEMAP_WIDTH_HEIGHT{1 << 9};
constexpr vk::Format LIGHTING_SHADOW_MAP_FORMAT{vk::Format::eD32Sfloat};

struct LightsInfo {
    std::uint32_t mLitCount;
    vk::DeviceAddress mLitIndices;
    std::uint32_t mShadowCastCount;
    vk::DeviceAddress mShadowCastIndices;
};

struct ResetPC : SwPC<ResetPC> {
    vk::DeviceAddress mShadowRcsBuffer{0};
    std::uint32_t mShadowRcsLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClusterBuildPC : SwPC<ClusterBuildPC> {
    vk::DeviceAddress mClustersBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClusterMarkActivePC : SwPC<ClusterMarkActivePC> {
    vk::DeviceAddress mActiveClustersIndicesBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClusterCullLightsPC : SwPC<ClusterCullLightsPC> {
    vk::DeviceAddress mFrameBuffer{0};
    vk::DeviceAddress mSceneLightsBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    vk::DeviceAddress mSceneLightsInfoBuffer{0};
    vk::DeviceAddress mActiveClustersIndicesBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct CullPC : SwPC<CullPC> {
    vk::DeviceAddress mShadowRcsBuffer{0};
    vk::DeviceAddress mShadowRisBuffer{0};
    vk::DeviceAddress mShadowRisIndicesBuffer{0};
    vk::DeviceAddress mSceneLightsBuffer{0};
    vk::DeviceAddress mSceneBoundsBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    vk::DeviceAddress mSceneLightsInfoBuffer{0};
    std::uint32_t mShadowRisLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mShadowRcsBuffer{0};
    vk::DeviceAddress mShadowRisIndicesBuffer{0};
    vk::DeviceAddress mSceneLightsBuffer{0};
    vk::DeviceAddress mSceneVertexBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    vk::DeviceAddress mSceneMaterialConstantsBuffer{0};
    vk::DeviceAddress mSceneLightsInfoBuffer{0};
    std::uint32_t mLightIndex{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    static SwDescriptorLayout sShadowConsumeDescriptorLayout;

    static void init();
    static void cleanup();

    std::array<SwDepthImage2D, MAX_NUM_SHADOW_CASTERS> mShadow2DMaps;
    std::array<SwDepthImageCubemap, MAX_NUM_SHADOW_CASTERS> mShadowCubeMaps;
    SwSampler mShadowMapsSampler;
    SwDescriptorSet mShadowMapsDescriptorSet;

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

    inline Resources& getResources() { return mResources; }
};

}  // namespace SwLighting
