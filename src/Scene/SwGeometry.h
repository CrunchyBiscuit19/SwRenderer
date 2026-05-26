#pragma once

#include <Resource/SwPushConstant.h>

#include <vulkan/vulkan.hpp>

namespace SwGeometry {
struct WorkPC : SwPC<WorkPC> {
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneMaterialConstantsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneVisibleRenderInstancesInstanceIndexBuffer;
    vk::DeviceAddress mPostCullRenderItemsBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    WorkPC mWorkPushConstants;
};
}  // namespace SwGeometry