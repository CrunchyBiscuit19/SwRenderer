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
struct Cluster {
    glm::vec3 mMin{0};
    glm::vec3 mMax{0};
};

static constexpr std::string_view SHADERS_PATH{SHADERS_DIR "/Lighting"};
static const std::filesystem::path CLUSTERS_BUILD_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "ClustersBuild.comp.spv"};
static const std::filesystem::path CLUSTERS_MARK_ACTIVE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "ClustersMarkActive.comp.spv"};
static const std::filesystem::path CLUSTERS_COMPACT_ACTIVE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "ClustersCompactActive.comp.spv"};
static const std::filesystem::path LIGHTS_CULL_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "LightsCull.comp.spv"};
static const std::filesystem::path CLUSTERS_LIGHT_SELECT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "ClustersLightSelect.comp.spv"};
static const std::filesystem::path RESET_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Reset.comp.spv"};
static const std::filesystem::path SHADOWS_CULL_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "ShadowsCull.comp.spv"};
static const std::filesystem::path SHADOWS_DRAW_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "ShadowsDraw.vert.spv"};
static constexpr std::string_view SHADOWS_DRAW_OPAQUE_ENTRY_POINT{"mainOpaque"};
static constexpr std::string_view SHADOWS_DRAW_MASKED_ENTRY_POINT{"mainMasked"};

static constexpr std::uint32_t MAX_NUM_SHADOW_CASTERS{16};
static constexpr std::uint32_t SHADOWS_2D_MAP_WIDTH_HEIGHT{1 << 10};
static constexpr std::uint32_t SHADOWS_CUBEMAP_WIDTH_HEIGHT{1 << 9};
static constexpr vk::Format SHADOWS_MAP_FORMAT{vk::Format::eD32Sfloat};
static constexpr vk::DeviceSize SHADOW_CAST_INDICES_BUFFER_SIZE{(1 + MAX_NUM_SHADOW_CASTERS) * sizeof(std::uint32_t)};

static constexpr glm::uvec3 CLUSTERS_DIMENSIONS{16, 9, 24};
static constexpr std::uint32_t NUM_CLUSTERS{CLUSTERS_DIMENSIONS.x * CLUSTERS_DIMENSIONS.y * CLUSTERS_DIMENSIONS.z};
static constexpr vk::DeviceSize CLUSTERS_BUFFER_SIZE{NUM_CLUSTERS * sizeof(Cluster)};
static constexpr vk::DeviceSize CLUSTERS_ACTIVE_BOOLEANS_BUFFER_SIZE{NUM_CLUSTERS * sizeof(bool)};
static constexpr vk::DeviceSize CLUSTERS_ACTIVE_INDICES_BUFFER_SIZE{(1 + NUM_CLUSTERS) * sizeof(std::uint32_t)};
static constexpr vk::DeviceSize CLUSTERS_LIGHT_COUNTS_SIZE{NUM_CLUSTERS * sizeof(std::uint32_t)};


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
    vk::DeviceAddress mFrameBuffer{0};
    vk::DeviceAddress mClustersBuffer{0};
    vk::DeviceAddress mClustersActiveBooleansBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClustersCompactActivePC : SwPC<ClustersCompactActivePC> {
    vk::DeviceAddress mClustersActiveIndicesBuffer{0};
    vk::DeviceAddress mClustersActiveBooleansBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct LightsCullPC : SwPC<LightsCullPC> {
    vk::DeviceAddress mFrameBuffer;
    vk::DeviceAddress mLightsBuffer{0};
    vk::DeviceAddress mNodeTransformsBuffer{0};
    vk::DeviceAddress mInstancesBuffer{0};
    vk::DeviceAddress mLitIndicesBuffer{0};
    vk::DeviceAddress mShadowCastsIndicesBuffer{0};
    std::uint32_t mLightsCount{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ClustersLightSelectPC : SwPC<ClustersLightSelectPC> {
    vk::DeviceAddress mFrameBuffer{0};
    vk::DeviceAddress mLightsBuffer{0};
    vk::DeviceAddress mNodeTransformsBuffer{0};
    vk::DeviceAddress mInstancesBuffer{0};
    vk::DeviceAddress mClustersActiveIndicesBuffer{0};
    vk::DeviceAddress mClustersLightIndicesBuffer{0};
    vk::DeviceAddress mClustersLightCounts{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowsCullPC : SwPC<ShadowsCullPC> {
    vk::DeviceAddress mShadowsRcsBuffer{0};
    vk::DeviceAddress mShadowsRisBuffer{0};
    vk::DeviceAddress mShadowsRisIndicesBuffer{0};
    vk::DeviceAddress mLightsBuffer{0};
    vk::DeviceAddress mBoundsBuffer{0};
    vk::DeviceAddress mNodeTransformsBuffer{0};
    vk::DeviceAddress mInstancesBuffer{0};
    std::uint32_t mShadowsRisLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ShadowDrawPC : SwPC<ShadowDrawPC> {  // MRTs then select the light's shadow map
    vk::DeviceAddress mShadowsRcsBuffer{0};
    vk::DeviceAddress mShadowsRisIndicesBuffer{0};
    vk::DeviceAddress mLightsBuffer{0};
    vk::DeviceAddress mVertexBuffer{0};
    vk::DeviceAddress mNodeTransformsBuffer{0};
    vk::DeviceAddress mInstancesBuffer{0};
    vk::DeviceAddress mMaterialConstantsBuffer{0};

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

    SwAllocatedBuffer mLitIndicesBuffer; // 1st 4 bytes as count
    SwAllocatedBuffer mShadowCastsIndicesBuffer;
    SwAllocatedBuffer mShadowsRcsBuffer;
    SwAllocatedBuffer mShadowsRisIndicesBuffer;

    SwAllocatedBuffer mClustersBuffer;
    SwAllocatedBuffer mClustersActiveBooleansBuffer; 
    SwAllocatedBuffer mClustersActiveIndicesBuffer; // 1st 4 bytes as count
    SwAllocatedBuffer mClustersLightIndicesBuffer;
    SwAllocatedBuffer mClustersLightCounts;

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

    LightsCullPC mLightsCullPc;
    SwPipelineLayout mLightsCullPipelineLayout;
    SwComputePipelineBundle mLightsCullPipelineBundle;

    ClustersLightSelectPC mClustersLightSelectPc;
    SwPipelineLayout mClustersLightSelectPipelineLayout;
    SwComputePipelineBundle mClustersLightSelectPipelineBundle;

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
    void refreshDataUsage() override;

public:
    System(SwScene& scene);

    void refresh() override;

    void regenerateShadowsRcs();

    inline SwDescriptorSet& getShadowsMapsDescriptorSet() { return mResources.mShadowsMapsDescriptorSet; }

    inline Resources& getResources() { return mResources; }
};

}  // namespace SwLighting
