#pragma once

#include <glm/glm.hpp>

struct Plane {
    glm::vec3 normal;
    float d;

    Plane() : normal(glm::vec3(0.f)), d(0.f) {}
    Plane(glm::vec3 n, glm::vec3 p) : normal(glm::normalize(n)), d(glm::dot(glm::normalize(n), p)) {}
};

namespace SwCull {
struct ResetPC {
    vk::DeviceAddress mPreCullRenderItemsBuffer;
    std::uint32_t mPreCullRenderItemsLimit;
};

struct WorkPC {
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
};

struct CompactPC {
    vk::DeviceAddress mPreCullRenderItemsBuffer;
    vk::DeviceAddress mPostCullRenderItemsBuffer;
    vk::DeviceAddress mPostCullRenderItemsCountBuffer;
    std::uint32_t mPreCullRenderItemsLimit;
};

struct DepthPyramidPC {
    glm::uvec2 mDepthPyramidExtent;
    glm::uvec2 mDepthFullExtent;
    glm::vec2 mDepthFullRatio;
    std::uint32_t mLevel;
    bool mReadFromFull;
};

struct Resources {
    SwPipelinePipeline mResetPipelinePipeline;
    SwPipelineLayout mResetPipelineLayout;
    SwCull::ResetPC mResetPushConstants;

    SwPipelinePipeline mCompactPipelinePipeline;
    SwPipelineLayout mCompactPipelineLayout;
    SwCull::CompactPC mCompactPushConstants;

    SwPipelinePipeline mDepthPyramidPipelinePipeline;
    SwPipelineLayout mDepthPyramidPipelineLayout;
    SwDescriptorSet mDepthPyramidDescriptorSet;
    SwDescriptorLayout mDepthPyramidDescriptorLayout;
    SwColorImage2D mDepthPyramidImage;
    std::uint32_t mDepthPyramidLevels{0};
    vk::Extent3D mDepthPyramidExtent;
    SwCull::DepthPyramidPC mDepthPyramidPushConstants;

    SwPipelinePipeline mWorkPipelinePipeline;
    SwPipelineLayout mWorkPipelineLayout;
    SwDescriptorSet mWorkDescriptorSet;
    SwDescriptorLayout mWorkDescriptorLayout;
    SwCull::WorkPC mWorkPushConstants;
};

};  // namespace SwCull