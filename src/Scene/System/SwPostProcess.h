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

namespace SwPostProcess {
static const std::filesystem::path TONEMAP_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwTonemap.comp.spv"};
static const std::filesystem::path FXAA_COMPUTE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwFXAA.comp.spv"};

struct TonemapPC : SwPC<TonemapPC> {
    float mExposure{1.f};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct FXAAPC : SwPC<FXAAPC> {
    glm::vec2 mInverseScreenSize;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct Resources {
    // Tonemap: HDR draw image resolved in place to LDR (always runs).
    SwComputePipelineBundle mTonemapPipelineBundle;
    SwPipelineLayout mTonemapPipelineLayout;
    SwDescriptorSet mTonemapDescriptorSet;
    SwDescriptorLayout mTonemapDescriptorLayout;
    SwPostProcess::TonemapPC mTonemapPushConstants;

    // FXAA: edge-aware anti-aliasing, in place on the (now LDR) draw image (toggleable).
    SwComputePipelineBundle mFXAAPipelineBundle;
    SwPipelineLayout mFXAAPipelineLayout;
    SwDescriptorSet mFXAADescriptorSet;
    SwDescriptorLayout mFXAADescriptorLayout;
    SwSampler mFXAASampler;
    SwPostProcess::FXAAPC mFXAAPushConstants;
};

class System : public SwSystem, public SwSystem::Resizable {
private:
    Resources mResources;

    bool mFXAAActive{true};

    void initializeResources() override;
    void initializePasses() override;

    void reInitializeOnResize() override;

public:
    System(SwScene& scene);

    inline Resources& getResources() { return mResources; }
    inline bool isFXAAActive() const { return mFXAAActive; }
    inline bool* getFXAAActivePtr() { return &mFXAAActive; }
    inline float* getExposurePtr() { return &mResources.mTonemapPushConstants.mExposure; }
};
}  // namespace SwPostProcess
