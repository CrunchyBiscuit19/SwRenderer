#pragma once

#include <Data/SwInstance.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace SwPick {
constexpr float PICK_IMGUIZMO_SIZE = 0.15f;

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneVisibleRenderInstancesInstanceIndexBuffer;
    vk::DeviceAddress mPostCullRenderItemsBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct ReadbackPC : SwPC<ReadbackPC> {
    vk::DeviceAddress mPickerBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ReadbackData {
    glm::ivec2 mCoords;
    glm::uvec2 mRead;
};

struct Resources {
    SwAllocatedBuffer mReadbackBuffer;

    SwColorImage2D mReadbackImage;
    SwDepthImage2D mDepthImage;

    SwDescriptorSet mReadbackDescriptorSet;
    SwDescriptorLayout mReadbackDescriptorLayout;

    DrawPC mDrawPushConstants;
    SwGraphicsPipelineBundle mDrawPipelineBundle;
    SwPipelineLayout mDrawPipelineLayout;

    ReadbackPC mReadbackPushConstants;
    SwComputePipelineBundle mReadbackPipelineBundle;
    SwPipelineLayout mReadbackPipelineLayout;

    ImGuizmo::OPERATION mImguizmoOperation;
    SwInstance* mClickedInstance;
};

}  // namespace SwPick
