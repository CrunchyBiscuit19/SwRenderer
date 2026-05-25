#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwSampler.h>

#include <glm/glm.hpp>

namespace SwSkybox {
constexpr std::uint32_t NUM_SKYBOX_VERTICES{36};

struct WorkPC {
    vk::DeviceAddress mWorkVertexBuffer;
    vk::DeviceAddress mPerFrameBuffer;
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

    std::optional<std::filesystem::path> mLoadFromDir;

    bool mActive{false};
};

};  // namespace SwSkybox