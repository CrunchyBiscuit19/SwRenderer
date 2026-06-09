#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Resource/SwSampler.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace SwFXAA {
static const std::filesystem::path FXAA_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwFXAAWork.comp.spv"};

struct WorkPC : SwPC<WorkPC> {
    glm::vec2 mInverseScreenSize;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct Resources {
    SwComputePipelineBundle mWorkPipelineBundle;
    SwPipelineLayout mWorkPipelineLayout;
    SwDescriptorSet mWorkDescriptorSet;
    SwDescriptorLayout mWorkDescriptorLayout;
    SwSampler mWorkSampler;
    SwFXAA::WorkPC mWorkPushConstants;
};

class System : public SwSystem, public SwSystem::Resizable {
private:
    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;

    void reInitializeOnResize() override;

public:
    System(SwScene& scene);

    inline Resources& getResources() { return mResources; }
};
}  // namespace SwFXAA
