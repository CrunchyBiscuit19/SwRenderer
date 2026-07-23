#pragma once

#include <Data/SwMaterial.h>
#include <Resource/SwPushConstant.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <vulkan/vulkan.hpp>

namespace SwGeometry {

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mVertexBuffer{0};
    vk::DeviceAddress mMaterialConstantsBuffer{0};
    vk::DeviceAddress mNodeTransformsBuffer{0};
    vk::DeviceAddress mInstancesBuffer{0};
    vk::DeviceAddress mRisIndicesBuffer{0};
    vk::DeviceAddress mRcsBuffer{0};
    vk::DeviceAddress mFrameBuffer{0};
    vk::DeviceAddress mLightsBuffer{0};
    vk::DeviceAddress mLitIndicesBuffer{0};
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
    void refreshDataUsage() override;

public:
    System(SwScene& scene);
};
}  // namespace SwGeometry