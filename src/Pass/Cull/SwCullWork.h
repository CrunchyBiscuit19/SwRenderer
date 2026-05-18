#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwPipeline.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct SwCullWorkPushConstants {
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

class SwCullWorkPass {
private:
    static std::filesystem::path CULL_WORK_COMPUTE_SHADER_PATH;

    static SwRendererContext sRendererContext;
    SwPipelinePipeline mWorkPipelineBundle;
    SwPipelineLayout mWorkPipelineLayout;
    SwDescriptorSet mWorkDescriptorSet;
    SwDescriptorLayout mWorkDescriptorLayout;
    SwCullWorkPushConstants mWorkPushConstants;

    //SwCullDepthPyramidPass& mDepthPyramidPass;

    void writeDescriptorSet();

    void writePushConstants();

public:
    SwCullWorkPass() = default;

    static void init(SwRendererContext rendererContext);

    void initialize();
};