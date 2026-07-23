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
    vk::DeviceAddress mRcsBuffer{0};
    std::uint32_t mRcsLimit{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct TestPC : public SwPC<TestPC> {
    vk::DeviceAddress mRcsBuffer{0};
    vk::DeviceAddress mRisBuffer{0};
    vk::DeviceAddress mBatchesBuffer{0};
    vk::DeviceAddress mRisCount{0};
    vk::DeviceAddress mFrameBuffer{0};
    vk::DeviceAddress mBoundsBuffer{0};
    vk::DeviceAddress mNodeTransformsBuffer{0};
    vk::DeviceAddress mInstancesBuffer{0};
    vk::DeviceAddress mRisIndicesBuffer{0};
    vk::DeviceAddress mRisVisibilityReadBuffer{0};
    vk::DeviceAddress mRisVisibilityWriteBuffer{0};
    std::uint32_t mRisLimit{0};
    Phase mPhase{Phase::Early};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct CompactPC : SwPC<CompactPC> {
    vk::DeviceAddress mBatchesBuffer{0};
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
    void refreshDataUsage() override;

    void reInitializeOnResize() override;

public:
    System(SwScene& scene);

    inline bool getFreeze() { return mFreeze; }
    inline bool* getFreezePtr() { return &mFreeze; }
};
};  // namespace SwCull