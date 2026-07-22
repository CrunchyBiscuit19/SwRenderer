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
#include <string_view>
#include <vulkan/vulkan.hpp>

namespace SwPick {
constexpr float IMGUIZMO_SIZE = 0.15f;
static constexpr std::string_view SHADERS_PATH{SHADERS_DIR "/Pick"};
static const std::filesystem::path DRAW_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Draw.vert.spv"};
static const std::filesystem::path DRAW_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Draw.frag.spv"};
static const std::filesystem::path READBACK_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Readback.comp.spv"};
constexpr std::string_view DRAW_OPAQUE_ENTRY_POINT{"mainOpaque"};
constexpr std::string_view DRAW_MASKED_ENTRY_POINT{"mainMasked"};

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mVertexBuffer{0};
    vk::DeviceAddress mMaterialConstantsBuffer{0};
    vk::DeviceAddress mNodeTransformsBuffer{0};
    vk::DeviceAddress mInstancesBuffer{0};
    vk::DeviceAddress mRisIndicesBuffer{0};
    vk::DeviceAddress mRcsBuffer{0};
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

    void refresh() override;

    void changePickOperation();
    void generatePickFrame();
    bool isPicked();

    inline Resources& getResources() { return mResources; }
    inline ImGuizmo::OPERATION getImguizmoOperation() { return mImguizmoOperation; }
    inline const std::optional<std::uint32_t>& getSelectedInstanceId() const { return mSelectedInstanceId; }
    inline void clearSelection() { mSelectedInstanceId.reset(); }
};
}  // namespace SwPick
