#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Resource/SwSampler.h>
#include <Scene/SwSystem.h>

#include <filesystem>

namespace SwCull {
static const std::filesystem::path CULL_SHADERS_DIR{std::filesystem::path(SHADERS_DIR) / "Cull"};
static const std::filesystem::path CULL_RESET_COMPUTE_SHADER_PATH{CULL_SHADERS_DIR / "SwCullReset.comp.spv"};
static const std::filesystem::path CULL_PREP_OCCLUSION_COMPUTE_SHADER_PATH{CULL_SHADERS_DIR / "SwCullPrepOcclusion.comp.spv"};
static const std::filesystem::path CULL_WORK_COMPUTE_SHADER_PATH{CULL_SHADERS_DIR / "SwCullWork.comp.spv"};
static const std::filesystem::path CULL_COMPACT_COMPUTE_SHADER_PATH{CULL_SHADERS_DIR / "SwCullCompact.comp.spv"};
static constexpr std::uint32_t CULL_MAX_DEPTH_PYRAMID_LEVELS{16};

enum class Phase { Early, Late };

struct ResetPC : public SwPC<ResetPC> {
    vk::DeviceAddress mSceneRcsBuffer;
    std::uint32_t mSceneRcsLimit;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct WorkPC : public SwPC<WorkPC> {
    vk::DeviceAddress mSceneRcsBuffer;
    vk::DeviceAddress mSceneRisBuffer;
    vk::DeviceAddress mStatsRisCount;
    vk::DeviceAddress mFrameBuffer;
    vk::DeviceAddress mSceneBoundsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneDrawRisIndicesBuffer;
    vk::DeviceAddress mSceneVisibilityRisReadBuffer;
    vk::DeviceAddress mSceneVisibilityRisWriteBuffer;
    std::uint32_t mSceneRisLimit;
    Phase mPhase;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct CompactPC : SwPC<CompactPC> {
    vk::DeviceAddress mPreRcsBuffer;
    vk::DeviceAddress mPostRcsBuffer;
    vk::DeviceAddress mPostRcsCount;
    std::uint32_t mPreRcsLimit;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct PrepOcclusionPC : SwPC<PrepOcclusionPC> {
    std::int32_t mLevel;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct Resources {
    SwComputePipelineBundle mResetPipelineBundle;
    SwPipelineLayout mResetPipelineLayout;
    SwCull::ResetPC mResetPushConstants;

    SwComputePipelineBundle mCompactPipelineBundle;
    SwPipelineLayout mCompactPipelineLayout;
    SwCull::CompactPC mCompactPushConstants;

    SwComputePipelineBundle mWorkPipelineBundle;
    SwPipelineLayout mWorkPipelineLayout;
    SwDescriptorSet mWorkDescriptorSet;
    SwDescriptorLayout mWorkDescriptorLayout;
    SwSampler mWorkDepthPyramidSampler;
    SwCull::WorkPC mWorkPushConstants;

    SwComputePipelineBundle mPrepOcclusionPipelineBundle;
    SwPipelineLayout mPrepOcclusionPipelineLayout;
    SwDescriptorSet mPrepOcclusionDescriptorSet;
    SwDescriptorLayout mPrepOcclusionDescriptorLayout;
    SwSampler mDepthPyramidMinSampler;
    SwColorImage2D mDepthPyramidImage;
    std::uint32_t mDepthPyramidLevels{0};
    SwCull::PrepOcclusionPC mPrepOcclusionPushConstants;
};

class System : public SwSystem, public SwSystem::Resizable {
private:
    Resources mResources;

    bool mFreeze{false};

    void initializeOtherPasses();
    void initializeEarlyPasses();
    void initializeLatePasses();

    void initializeResources() override;
    void initializePasses() override;
    void initializePushConstants() override;

    void reInitializeOnResize() override;

public:
    System(SwScene& scene);

    inline bool getFreeze() { return mFreeze; }
    inline bool* getFreezePtr() { return &mFreeze; }

    void refreshDynamicDependencies() override;
    void refreshPushConstants() override;

    void refresh() override;
};
};  // namespace SwCull