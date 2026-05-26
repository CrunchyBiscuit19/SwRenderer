#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>

#include <glm/glm.hpp>

namespace SwCull {
struct Plane {
    glm::vec3 normal;
    float d;

    Plane() : normal(glm::vec3(0.f)), d(0.f) {}
    Plane(glm::vec3 n, glm::vec3 p) : normal(glm::normalize(n)), d(glm::dot(glm::normalize(n), p)) {}
};

struct ResetPC : public SwPC<ResetPC> {
    vk::DeviceAddress mPreCullRenderItemsBuffer;
    std::uint32_t mPreCullRenderItemsLimit;

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

struct DepthPyramidPC : SwPC<DepthPyramidPC> {
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

};  // namespace SwCull