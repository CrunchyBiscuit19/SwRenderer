#pragma once

#include <Data/SwMaterial.h>
#include <Resource/SwPushConstant.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <vulkan/vulkan.hpp>

namespace SwGeometry {

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneMaterialConstantsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneRisIndicesBuffer;
    vk::DeviceAddress mSceneRcsBuffer;
    vk::DeviceAddress mFrameBuffer;
    vk::DeviceAddress mSceneLightsBuffer;
    vk::DeviceAddress mVisibleLightsBuffer;
    float mMaxPrefilterMipLevel;
    float mIblIntensity;
    std::uint32_t mIblComponents;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
};

struct Resources {
    DrawPC mDrawPushConstants;

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