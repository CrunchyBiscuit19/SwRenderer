#pragma once

#include <Data/SwMaterial.h>
#include <Resource/SwPushConstant.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <vulkan/vulkan.hpp>

namespace SwGeometry {

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mSceneVertexBuffer{0};
    vk::DeviceAddress mSceneMaterialConstantsBuffer{0};
    vk::DeviceAddress mSceneNodeTransformsBuffer{0};
    vk::DeviceAddress mSceneInstancesBuffer{0};
    vk::DeviceAddress mSceneRisIndicesBuffer{0};
    vk::DeviceAddress mSceneRcsBuffer{0};
    vk::DeviceAddress mFrameBuffer{0};
    vk::DeviceAddress mSceneLightsBuffer{0};
    vk::DeviceAddress mSceneLightsInfoBuffer{0};
    float mMaxPrefilterMipLevel{0.f};
    float mIblIntensity{0.f};
    std::uint32_t mIblComponents{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
};

struct Resources {
    DrawPC mDrawPushConstants;

    static constexpr std::string_view ZPASS_MASKED_ENTRY_POINT{"mainZPassMasked"};
    SwGraphicsPipelineBundle mZPassOpaquePipelineBundle;
    SwGraphicsPipelineBundle mZPassMaskedPipelineBundle;

    static std::array<vk::DescriptorSetLayout, 4> sGeometrySetLayouts;

    static void init();
    static void cleanup();
};

class System : public SwSystem {
private:
    Resources mResources;

    void drawBatches(vk::CommandBuffer cmd, std::array<std::optional<SwMaterial::Type>, SwMaterial::NUM_TYPES> matTypes, bool early);
    void drawZBatches(vk::CommandBuffer cmd, SwGraphicsPipelineBundle& pipeline, SwMaterial::Type matType, bool bindMaterialSets);

    void initializeResources() override;
    void initializePasses() override;
    void refreshDependencies() override;
    void refreshPushConstants() override;

public:
    System(SwScene& scene);
};
}  // namespace SwGeometry