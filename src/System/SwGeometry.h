#pragma once

#include <Data/SwMaterial.h>
#include <Resource/SwPushConstant.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <vulkan/vulkan.hpp>

namespace SwGeometry {

struct WorkPC : SwPC<WorkPC> {
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneMaterialConstantsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneRisIndicesBuffer;
    vk::DeviceAddress mSceneRcsBuffer;
    vk::DeviceAddress mFrameBuffer;
    vk::DeviceAddress mSceneLightsBuffer;
    vk::DeviceAddress mVisibleLightsBuffer;
    float mMaxPrefilterMip;        // Highest mip index of the SwIBL specular prefilter chain
    float mIblIntensity;           // Scales the image-based ambient term (GUI-controlled)
    std::uint32_t mIblComponents;  // IBL diffuse / specular bit mask (GUI-controlled)

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
};

struct Resources {
    WorkPC mWorkPushConstants;

    static std::array<vk::DescriptorSetLayout, 3> sGeometrySetLayouts;

    static void init();
    static void cleanup();
};

class System : public SwSystem {
private:
    Resources mResources;

    void drawBatches(vk::CommandBuffer cmd, std::array<std::optional<SwMaterial::Type>, SwMaterial::NUM_TYPES> matTypes, bool early);

    void initializeResources() override;
    void initializePasses() override;
    void refreshDependencies() override;
    void refreshPushConstants() override;

public:
    System(SwScene& scene);
};
}  // namespace SwGeometry