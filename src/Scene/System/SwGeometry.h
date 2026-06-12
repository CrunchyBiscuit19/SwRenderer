#pragma once

#include <Resource/SwPushConstant.h>
#include <Scene/SwSystem.h>

#include <vulkan/vulkan.hpp>

#include <filesystem>

namespace SwGeometry {

struct WorkPC : SwPC<WorkPC> {
    vk::DeviceAddress mSceneVertexBuffer;
    vk::DeviceAddress mSceneMaterialConstantsBuffer;
    vk::DeviceAddress mSceneNodeTransformsBuffer;
    vk::DeviceAddress mSceneInstancesBuffer;
    vk::DeviceAddress mSceneDrawRisIndicesBuffer;
    vk::DeviceAddress mDrawRcsBuffer;
    vk::DeviceAddress mPerFrameBuffer;
    vk::DeviceAddress mSceneLightsBuffer;
    std::uint32_t mLightCount;
    float mMaxPrefilterMip;  // highest mip index of the SwIBL specular prefilter chain
    float mIblIntensity;     // scales the image-based ambient term (GUI-controlled)

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
};

struct Resources {
    WorkPC mWorkPushConstants;
};

class System : public SwSystem {
private:

    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;

public:
    System(SwScene& scene);

    void refreshDynamicDependencies() override;
    void refreshPushConstants() override;
};
}  // namespace SwGeometry