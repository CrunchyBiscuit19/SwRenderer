#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Resource/SwSampler.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <string_view>

namespace SwCull {
static constexpr std::string_view SHADERS_PATH{SHADERS_DIR "/Cull"};
static const std::filesystem::path RESET_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Reset.comp.spv"};
static const std::filesystem::path PREP_OCCLUSION_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "PrepOcclusion.comp.spv"};
static const std::filesystem::path TEST_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Test.comp.spv"};
static const std::filesystem::path COMPACT_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Compact.comp.spv"};
static constexpr std::uint32_t MAX_DEPTH_PYRAMID_LEVELS{16};

enum class Phase { Early, Late };

struct ResetPC : public SwPC<ResetPC> {
    vk::DeviceAddress mSceneRcsBuffer{0};
    std::uint32_t mSceneRcsLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct TestPC : public SwPC<TestPC> {
    vk::DeviceAddress mSceneRcsBuffer{0};
    vk::DeviceAddress mSceneRisBuffer{0};
    vk::DeviceAddress mSceneBatchesBuffer{0};
    vk::DeviceAddress mSceneRisCount{0};
    vk::DeviceAddress mFrameBuffer{0};
    vk::DeviceAddress mSceneBoundsBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    vk::DeviceAddress mSceneRisIndicesBuffer{0};
    vk::DeviceAddress mSceneRisVisibilityReadBuffer{0};
    vk::DeviceAddress mSceneRisVisibilityWriteBuffer{0};
    std::uint32_t mSceneRisLimit{0};
    Phase mPhase{Phase::Early};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct CompactPC : SwPC<CompactPC> {
    vk::DeviceAddress mSceneBatchesBuffer{0};
    vk::DeviceAddress mPreRcsBuffer{0};
    vk::DeviceAddress mPostRcsBuffer{0};
    vk::DeviceAddress mPostRcsCount{0};
    std::uint32_t mPreRcsLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct PrepOcclusionPC : SwPC<PrepOcclusionPC> {
    std::int32_t mLevel{-1};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct Resources {
    SwComputePipelineBundle mResetPipelineBundle;
    SwPipelineLayout mResetPipelineLayout;
    SwCull::ResetPC mResetPushConstants;

    SwComputePipelineBundle mTestPipelineBundle;
    SwPipelineLayout mTestPipelineLayout;
    SwDescriptorSet mTestDescriptorSet;
    SwDescriptorLayout mTestDescriptorLayout;
    SwSampler mTestDepthPyramidSampler;
    SwCull::TestPC mTestPushConstants;

    SwComputePipelineBundle mCompactPipelineBundle;
    SwPipelineLayout mCompactPipelineLayout;
    SwCull::CompactPC mCompactPushConstants;

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
    void refreshDependencies() override;
    void refreshPushConstants() override;

    void reInitializeOnResize() override;

public:
    System(SwScene& scene);

    inline bool getFreeze() { return mFreeze; }
    inline bool* getFreezePtr() { return &mFreeze; }
};
};  // namespace SwCull