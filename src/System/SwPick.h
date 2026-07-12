#pragma once

#include <Data/SwInstance.h>
#include <Data/SwMaterial.h>
#include <ImGuizmo.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Scene/SwSystem.h>
#include <imgui.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace SwPick {
constexpr float PICK_IMGUIZMO_SIZE = 0.15f;
static const std::filesystem::path PICK_SHADERS_DIR{std::filesystem::path(SHADERS_DIR) / "Pick"};
static const std::filesystem::path PICK_DRAW_VERTEX_SHADER_PATH{PICK_SHADERS_DIR / "SwPickDraw.vert.spv"};
static const std::filesystem::path PICK_DRAW_FRAGMENT_SHADER_PATH{PICK_SHADERS_DIR / "SwPickDraw.frag.spv"};
static const std::filesystem::path PICK_READBACK_COMPUTE_SHADER_PATH{PICK_SHADERS_DIR / "SwPickReadback.comp.spv"};
constexpr std::string_view PICK_DRAW_OPAQUE_ENTRY_POINT{"mainOpaque"};
constexpr std::string_view PICK_DRAW_MASKED_ENTRY_POINT{"mainMasked"};

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mSceneVertexBuffer{0};
    vk::DeviceAddress mSceneMaterialConstantsBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    vk::DeviceAddress mSceneRisIndicesBuffer{0};
    vk::DeviceAddress mSceneRcsBuffer{0};
    vk::DeviceAddress mFrameBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct ReadbackPC : SwPC<ReadbackPC> {
    vk::DeviceAddress mReadbackBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct ReadbackData {
    glm::ivec2 mCoords{0};
    glm::uvec2 mRead{0};
};

struct Resources {
    SwAllocatedBuffer mReadbackBuffer;
    SwColorImage2D mReadbackImage;
    SwDescriptorSet mReadbackDescriptorSet;
    SwDescriptorLayout mReadbackDescriptorLayout;
    ReadbackPC mReadbackPushConstants;
    SwComputePipelineBundle mReadbackPipelineBundle;
    SwPipelineLayout mReadbackPipelineLayout;

    DrawPC mDrawPushConstants;
    SwGraphicsPipelineBundle mDrawOpaqueTransparentPipelineBundle;
    SwGraphicsPipelineBundle mDrawMaskedPipelineBundle;
    SwPipelineLayout mDrawPipelineLayout;
};
class System : public SwSystem, public SwSystem::Resizable {
private:
    Resources mResources;

    ImGuizmo::OPERATION mImguizmoOperation{ImGuizmo::TRANSLATE};
    std::optional<std::uint32_t> mSelectedInstanceId;

    void drawBatches(
        vk::CommandBuffer cmd, std::array<std::optional<SwMaterial::Type>, SwMaterial::NUM_TYPES> matTypes, SwGraphicsPipelineBundle& pipeline, bool early
    );

    void initializeResources() override;
    void initializePasses() override;
    void refreshDependencies() override;
    void refreshPushConstants() override;

    void reInitializeOnResize() override;

public:
    System(SwScene& scene);

    void changePickOperation();
    void generatePickFrame();
    bool isPicked();

    inline Resources& getResources() { return mResources; }
    inline ImGuizmo::OPERATION getImguizmoOperation() { return mImguizmoOperation; }
    inline const std::optional<std::uint32_t>& getSelectedInstanceId() const { return mSelectedInstanceId; }
    inline void clearSelection() { mSelectedInstanceId.reset(); }
};
}  // namespace SwPick
