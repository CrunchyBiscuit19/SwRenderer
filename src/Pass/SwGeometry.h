#pragma once

#include <vulkan/vulkan.hpp>

namespace SwGeometry {
struct WorkPC {
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneMaterialConstantsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneVisibleRenderInstancesInstanceIndexBuffer;
    vk::DeviceAddress mPostCullRenderItemsBuffer;
};

struct Resources {
    WorkPC mWorkPushConstants;
};
}  // namespace SwGeometry