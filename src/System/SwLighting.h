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
static const std::filesystem::path LIGHTING_CLUSTERS_BUILD_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingClustersBuild.comp.spv"};
static const std::filesystem::path LIGHTING_CLUSTERS_MARK_ACTIVE_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingClustersMarkActive.comp.spv"};
static const std::filesystem::path LIGHTING_CLUSTERS_COMPACT_ACTIVE_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingClustersCompactActive.comp.spv"};
static const std::filesystem::path LIGHTING_CLUSTERS_CULL_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingClustersCull.comp.spv"};
static const std::filesystem::path LIGHTING_RESET_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingReset.comp.spv"};
static const std::filesystem::path LIGHTING_SHADOWS_CULL_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowsCull.comp.spv"};
static const std::filesystem::path LIGHTING_SHADOWS_DRAW_VERTEX_SHADER_PATH{LIGHTING_SHADERS_DIR / "SwLightingShadowsDraw.vert.spv"};
static constexpr std::string_view LIGHTING_SHADOWS_DRAW_OPAQUE_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view LIGHTING_SHADOWS_DRAW_MASKED_ENTRY_POINT{"mainMasked"};

static constexpr std::uint32_t MAX_NUM_SHADOW_CASTERS{16};
static constexpr std::uint64_t LIGHTING_INITIAL_SHADOWS_RENDER_COMMANDS_BUFFER_SIZE{(1 << 10) * sizeof(SwRenderCommand)};
static constexpr std::uint64_t LIGHTING_INITIAL_SHADOWS_RENDER_ITEMS_INDICES_BUFFER_SIZE{(1 << 12) * sizeof(std::uint32_t)};
static constexpr std::uint32_t LIGHTING_SHADOWS_2D_MAP_WIDTH_HEIGHT{1 << 10};
static constexpr std::uint32_t LIGHTING_SHADOWS_CUBEMAP_WIDTH_HEIGHT{1 << 9};
static constexpr vk::Format LIGHTING_SHADOWS_MAP_FORMAT{vk::Format::eD32Sfloat};
static constexpr glm::uvec3 LIGHTING_CLUSTERS_DIMENSIONS{16, 9, 24};
static constexpr std::uint32_t LIGHTING_NUM_CLUSTERS{3456};

struct Cluster {
    glm::vec3 mMin{0};
    glm::vec3 mMax{0};
};

static constexpr std::uint64_t LIGHTING_INITIAL_CLUSTERS_BUFFER_SIZE{LIGHTING_NUM_CLUSTERS * sizeof(Cluster)};
static constexpr std::uint64_t LIGHTING_INITIAL_CLUSTERS_ACTIVE_BOOLEANS_BUFFER_SIZE{LIGHTING_NUM_CLUSTERS * sizeof(std::uint32_t)};

struct LightsInfo {
    std::uint32_t mLitCount{0};
    vk::DeviceAddress mLitIndices{0};
    std::uint32_t mShadowCastCount{0};
    vk::DeviceAddress mShadowCastIndices{0};
};

struct ResetPC : SwPC<ResetPC> {
    vk::DeviceAddress mShadowsRcsBuffer{0};
    std::uint32_t mShadowsRcsLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClustersBuildPC : SwPC<ClustersBuildPC> {
    vk::DeviceAddress mFrameBuffer{0};
    vk::DeviceAddress mClustersBuffer{0};
    glm::mat4 mInvProj{1.f};
    glm::uvec2 mTargetSize{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClustersMarkActivePC : SwPC<ClustersMarkActivePC> {
    vk::DeviceAddress mClustersBuffer{0};
    vk::DeviceAddress mClustersActiveBooleansBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClustersCompactActivePC : SwPC<ClustersCompactActivePC> {
    vk::DeviceAddress mClustersActiveIndicesBuffer{0};
    vk::DeviceAddress mClustersActiveBooleansBuffer{0};
    vk::DeviceAddress mClustersActiveCount{0};

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

struct ShadowsCullPC : SwPC<ShadowsCullPC> {
    vk::DeviceAddress mShadowsRcsBuffer{0};
    vk::DeviceAddress mShadowsRisBuffer{0};
    vk::DeviceAddress mShadowsRisIndicesBuffer{0};
    vk::DeviceAddress mSceneLightsBuffer{0};
    vk::DeviceAddress mSceneLightsInfoBuffer{0};
    vk::DeviceAddress mSceneBoundsBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    std::uint32_t mShadowsRisLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowDrawPC : SwPC<ShadowDrawPC> {  // MRTs then select the light's shadow map
    vk::DeviceAddress mShadowsRcsBuffer{0};
    vk::DeviceAddress mShadowsRisIndicesBuffer{0};
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

    SwAllocatedBuffer mShadowsRcsBuffer;
    SwAllocatedBuffer mShadowsRisIndicesBuffer;

    SwAllocatedBuffer mClustersBuffer;
    SwAllocatedBuffer mClustersActiveBooleansBuffer;
    SwAllocatedBuffer mClustersActiveIndicesBuffer;
    SwAllocatedBuffer mClustersActiveCount;

    ResetPC mResetPc;
    SwPipelineLayout mResetPipelineLayout;
    SwComputePipelineBundle mResetPipelineBundle;

    ClustersBuildPC mClustersBuildPc;
    SwPipelineLayout mClustersBuildPipelineLayout;
    SwComputePipelineBundle mClustersBuildPipelineBundle;

    ClustersMarkActivePC mClustersMarkActivePc;
    SwDescriptorLayout mClustersMarkActiveDescriptorLayout;
    SwDescriptorSet mClustersMarkActiveDescriptorSet;
    SwPipelineLayout mClustersMarkActivePipelineLayout;
    SwComputePipelineBundle mClustersMarkActivePipelineBundle;

    ClustersCompactActivePC mClustersCompactActivePc;
    SwPipelineLayout mClustersCompactActivePipelineLayout;
    SwComputePipelineBundle mClustersCompactActivePipelineBundle;

    ClustersCullPC mClustersCullPc;
    SwPipelineLayout mClustersCullPipelineLayout;
    SwComputePipelineBundle mClustersCullPipelineBundle;

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

    void refresh() override;

    void regenerateShadowsRcs();

    inline SwDescriptorSet& getShadowsMapsDescriptorSet() { return mResources.mShadowsMapsDescriptorSet; }

    inline Resources& getResources() { return mResources; }
};

}  // namespace SwLighting
