#pragma once

#include <Data/SwInstance.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace SwPick {
constexpr float PICK_IMGUIZMO_SIZE = 0.15f;
static const std::filesystem::path PICK_DRAW_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwPickDraw.vert.spv"};
static const std::filesystem::path PICK_DRAW_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwPickDraw.frag.spv"};
static const std::filesystem::path PICK_READBACK_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwPickReadback.comp.spv"};

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneDrawRisIndicesBuffer;
    vk::DeviceAddress mDrawRcsBuffer;
    vk::DeviceAddress mPerFrameBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct ReadbackPC : SwPC<ReadbackPC> {
    vk::DeviceAddress mReadbackBuffer;

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

    ImGuizmo::OPERATION mImguizmoOperation{ImGuizmo::TRANSLATE};
    SwInstance* mSelectedInstance{nullptr};
};
class System : public SwSystem, public SwSystem::Resizable {
private:
    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;
    void initializePushConstants() override;

    void reInitializeOnResize() override;

public:
    System(SwScene& scene);

    void changePickOperation();
    void generatePickFrame();
    bool isPicked();

    inline Resources& getResources() { return mResources; }

    void refreshDynamicDependencies() override;
    void refreshPushConstants() override;
};
}  // namespace SwPick
