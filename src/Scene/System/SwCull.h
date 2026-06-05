#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Resource/SwSampler.h>
#include <Scene/SwSystem.h>
#include <glm/glm.hpp>
#include <filesystem>

namespace SwCull {
static const std::filesystem::path CULL_RESET_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwCullReset.comp.spv"};
static const std::filesystem::path CULL_PREP_OCCLUSION_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwCullPrepOcclusion.comp.spv"};
static const std::filesystem::path CULL_WORK_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwCullWork.comp.spv"};
static const std::filesystem::path CULL_COMPACT_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwCullCompact.comp.spv"};
static constexpr std::uint32_t CULL_MAX_DEPTH_PYRAMID_LEVELS{16};

struct Plane {
    glm::vec3 mNormal;
    float mDistance;

    Plane() : mNormal(glm::vec3(0.f)), mDistance(0.f) {}
    Plane(glm::vec3 n, glm::vec3 p) : mNormal(glm::normalize(n)), mDistance(glm::dot(glm::normalize(n), p)) {}
};

enum class Phase { Frustum, Occlusion };

struct ResetPC : public SwPC<ResetPC> {
    vk::DeviceAddress mRItemsBuffer;
    std::uint32_t mRItemsLimit;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct WorkPC : public SwPC<WorkPC> {
    vk::DeviceAddress mRItemsBuffer;
    vk::DeviceAddress mRInstsBuffer;
    vk::DeviceAddress mRInstsCount;
    vk::DeviceAddress mFrustumBuffer;
    vk::DeviceAddress mPerFrameBuffer;
    vk::DeviceAddress mSceneBoundsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneVisibleRInstsIndicesBuffer;
    std::uint32_t mRInstsLimit;
    glm::vec2 mDrawExtents;
    glm::uvec2 mDepthPyramidExtents;
    Phase mPhase;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct CompactPC : SwPC<CompactPC> {
    vk::DeviceAddress mPreRItemsBuffer;
    vk::DeviceAddress mPostRItemsBuffer;
    vk::DeviceAddress mPostRItemsCount;
    std::uint32_t mPreRItemsLimit;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct PrepOcclusionPC : SwPC<PrepOcclusionPC> {
    glm::uvec2 mDepthPyramidExtent;
    glm::uvec2 mDepthFullExtent;
    glm::vec2 mDepthFullRatio;
    std::uint32_t mLevel;
    bool mReadFromFull;

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
};
};  // namespace SwCull