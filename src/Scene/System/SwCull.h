#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Scene/SwSystem.h>
#include <glm/glm.hpp>
#include <filesystem>

namespace SwCull {
static const std::filesystem::path CULL_RESET_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwCullReset.comp.spv"};
static const std::filesystem::path CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwCullDepthPyramid.comp.spv"};
static const std::filesystem::path CULL_WORK_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwCullWork.comp.spv"};
static const std::filesystem::path CULL_COMPACT_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwCullCompact.comp.spv"};
static constexpr std::uint32_t CULL_MAX_DEPTH_PYRAMID_LEVELS{16};

struct Plane {
    glm::vec3 mNormal;
    float mDistance;

    Plane() : mNormal(glm::vec3(0.f)), mDistance(0.f) {}
    Plane(glm::vec3 n, glm::vec3 p) : mNormal(glm::normalize(n)), mDistance(glm::dot(glm::normalize(n), p)) {}
};

struct ResetPC : public SwPC<ResetPC> {
    vk::DeviceAddress mPreCullRenderItemsBuffer;
    std::uint32_t mPreCullRenderItemsLimit;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct DepthPyramidPC : SwPC<DepthPyramidPC> {
    glm::uvec2 mDepthPyramidExtent;
    glm::uvec2 mDepthFullExtent;
    glm::vec2 mDepthFullRatio;
    std::uint32_t mLevel;
    bool mReadFromFull;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct WorkPC : public SwPC<WorkPC> {
    vk::DeviceAddress mPreCullRenderItemsBuffer;
    vk::DeviceAddress mRenderInstancesBuffer;
    vk::DeviceAddress mRenderInstancesCountBuffer;
    vk::DeviceAddress mFrustumBuffer;
    vk::DeviceAddress mPerFrameBuffer;
    vk::DeviceAddress mSceneBoundsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneVisibleRenderInstancesInstanceIndexBuffer;
    std::uint32_t mRenderInstancesLimit;
    glm::vec2 mDrawExtents;
    glm::vec2 mDepthPyramidExtents;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct CompactPC : SwPC<CompactPC> {
    vk::DeviceAddress mPreCullRenderItemsBuffer;
    vk::DeviceAddress mPostCullRenderItemsBuffer;
    vk::DeviceAddress mPostCullRenderItemsCountBuffer;
    std::uint32_t mPreCullRenderItemsLimit;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct Resources {
    SwComputePipelineBundle mResetPipelineBundle;
    SwPipelineLayout mResetPipelineLayout;
    SwCull::ResetPC mResetPushConstants;

    SwComputePipelineBundle mCompactPipelineBundle;
    SwPipelineLayout mCompactPipelineLayout;
    SwCull::CompactPC mCompactPushConstants;

    SwComputePipelineBundle mDepthPyramidPipelineBundle;
    SwPipelineLayout mDepthPyramidPipelineLayout;
    SwDescriptorSet mDepthPyramidDescriptorSet;
    SwDescriptorLayout mDepthPyramidDescriptorLayout;
    SwColorImage2D mDepthPyramidImage;
    std::uint32_t mDepthPyramidLevels{0};
    vk::Extent3D mDepthPyramidExtent;
    SwCull::DepthPyramidPC mDepthPyramidPushConstants;

    SwComputePipelineBundle mWorkPipelineBundle;
    SwPipelineLayout mWorkPipelineLayout;
    SwDescriptorSet mWorkDescriptorSet;
    SwDescriptorLayout mWorkDescriptorLayout;
    SwCull::WorkPC mWorkPushConstants;
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

    inline bool* getFreezePtr() { return &mFreeze; }

    void refreshDynamicDependencies() override;
    void refreshPushConstants() override;
};
};  // namespace SwCull