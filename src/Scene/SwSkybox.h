#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwSampler.h>
#include <Resource/SwPushConstant.h>
#include <Scene/SwSystem.h>
#include <filesystem>
#include <glm/glm.hpp>

class SwScene;

namespace SwSkybox {
constexpr std::uint32_t NUM_SKYBOX_VERTICES{36};
static const std::filesystem::path SKYBOX_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwSkyboxWork.vert.spv"};
static const std::filesystem::path SKYBOX_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwSkyboxWork.frag.spv"};
static const std::filesystem::path SKYBOX_DEFAULT_DIRECTORY_PATH{std::filesystem::path(SKYBOXES_PATH) / "ocean/"};

struct WorkPC : SwPC<WorkPC> {
    vk::DeviceAddress mWorkVertexBuffer;
    vk::DeviceAddress mPerFrameBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    SwColorImageCubemap mWorkImage;

    SwSampler mWorkSampler;

    const std::vector<float> mWorkVertices = {
        -1.0f, 1.0f,  -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
        1.0f,  -1.0f, -1.0f, 1.0f, 1.0f,  1.0f,  -1.0f, 1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,

        -1.0f, -1.0f, 1.0f,  1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,
        -1.0f, 1.0f,  -1.0f, 1.0f, -1.0f, 1.0f,  1.0f,  1.0f, -1.0f, -1.0f, 1.0f,  1.0f,

        1.0f,  -1.0f, -1.0f, 1.0f, 1.0f,  -1.0f, 1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  -1.0f, 1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

        -1.0f, -1.0f, 1.0f,  1.0f, -1.0f, 1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  -1.0f, 1.0f,  1.0f, -1.0f, -1.0f, 1.0f,  1.0f,

        -1.0f, 1.0f,  -1.0f, 1.0f, 1.0f,  1.0f,  -1.0f, 1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f, -1.0f, 1.0f,  1.0f,  1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,

        -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
        1.0f,  -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,
    };
    SwAllocatedBuffer mWorkVertexBuffer;

    SwGraphicsPipelineBundle mWorkPipelineBundle;
    SwPipelineLayout mWorkPipelineLayout;

    SwDescriptorSet mWorkDescriptorSet;
    SwDescriptorLayout mWorkDescriptorLayout;

    WorkPC mWorkPushConstants;
};

class System : public SwSystem {
private:
    Resources mResources;
    std::optional<std::filesystem::path> mLoadFromDir{std::nullopt};
    bool mActive{false};

    void initializeResources() override;
    void initializePasses() override;

public:
    System(SwScene& scene);

    inline void toggleActive() { mActive = !mActive; }
    inline bool isActive() const { return mActive; }
    inline bool isDirSelected() const { return mLoadFromDir.has_value(); }

    void reinitializeOnUpdate(std::optional<std::filesystem::path>);

    void refreshPushConstants() override;
};

};  // namespace SwSkybox