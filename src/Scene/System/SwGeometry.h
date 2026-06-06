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
    vk::DeviceAddress mSceneVisibleRInstsIndicesBuffer;
    vk::DeviceAddress mDrawRItemsBuffer;
    vk::DeviceAddress mPerFrameBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
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