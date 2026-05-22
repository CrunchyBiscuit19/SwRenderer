#pragma once

#include <Data/SwInstance.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace SwPick {
constexpr float PICK_IMGUIZMO_SIZE = 0.15f;

struct DrawPC {
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneVisibleRenderInstancesInstanceIndexBuffer;
    vk::DeviceAddress mPostCullRenderItemsBuffer;
};

struct WorkPC {
    vk::DeviceAddress mPickerBuffer;
};

struct Data {
    glm::ivec2 mCoords;
    glm::uvec2 mRead;
};

struct Resources {
    SwAllocatedBuffer mWorkBuffer;

    SwColorImage2D mWorkImage;
    SwDepthImage2D mDepthImage;

    SwDescriptorSet mDescriptorSet;
    SwDescriptorLayout mDescriptorSetLayout;

    DrawPC mDrawPushConstants;
    SwPipelinePipeline mDrawPipelinePipeline;
    SwPipelineLayout mDrawPipelineLayout;

    WorkPC mWorkPushConstants;
    SwPipelinePipeline mWorkPipelinePipeline;
    SwPipelineLayout mWorkPipelineLayout;

    ImGuizmo::OPERATION mImguizmoOperation;
    SwInstance* mClickedInstance;
};

}  // namespace SwPick
