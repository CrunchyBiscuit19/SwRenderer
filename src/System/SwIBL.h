#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Resource/SwSampler.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <vector>
#include <vulkan/vulkan.hpp>

class SwScene;

namespace SwIBL {
static const std::filesystem::path IRRADIANCE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwIBLIrradiance.comp.spv"};
static const std::filesystem::path PREFILTER_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwIBLPrefilter.comp.spv"};
static const std::filesystem::path BRDF_LUT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwIBLBrdfLut.comp.spv"};

// HDR float maps so the prefilter/irradiance integrals stay meaningful (the environment is HDR).
constexpr vk::Format IBL_FORMAT{vk::Format::eR16G16B16A16Sfloat};
constexpr vk::Format BRDF_LUT_FORMAT{vk::Format::eR16G16Sfloat};

constexpr vk::Extent3D IRRADIANCE_EXTENT{64, 32, 1};
constexpr vk::Extent3D PREFILTER_EXTENT{128, 64, 1};  // mip-chained; mip level encodes roughness
constexpr vk::Extent3D BRDF_LUT_EXTENT{512, 512, 1};

// Set-1 bindings consumed by the geometry/transparent fragment shaders.
constexpr std::uint32_t CONSUME_IRRADIANCE_BINDING{0};
constexpr std::uint32_t CONSUME_PREFILTER_BINDING{1};
constexpr std::uint32_t CONSUME_BRDF_LUT_BINDING{2};

struct PrefilterPC : SwPC<PrefilterPC> {
    float mRoughness;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct Resources {
    // Baked maps. Irradiance + BRDF LUT are single-level; prefilter is mip-chained (roughness per mip).
    SwColorImage2D mIrradianceImage;
    SwColorImage2D mPrefilterImage;
    SwColorImage2D mBrdfLutImage;

    SwSampler mEnvSampler;  // equirect maps: repeat longitude (U), clamp latitude (V), trilinear for the prefilter mips
    SwSampler mLutSampler;  // BRDF LUT: clamp both axes

    // BRDF LUT bake (one storage-image output, no environment input).
    SwComputePipelineBundle mBrdfLutPipelineBundle;
    SwPipelineLayout mBrdfLutPipelineLayout;
    SwDescriptorLayout mBrdfLutDescriptorLayout;
    SwDescriptorSet mBrdfLutDescriptorSet;

    // Irradiance + prefilter share the same bake-input layout: binding 0 = environment sampler, binding 1 = storage output.
    SwDescriptorLayout mBakeInputDescriptorLayout;

    SwComputePipelineBundle mIrradiancePipelineBundle;
    SwPipelineLayout mIrradiancePipelineLayout;
    SwDescriptorSet mIrradianceDescriptorSet;

    SwComputePipelineBundle mPrefilterPipelineBundle;
    SwPipelineLayout mPrefilterPipelineLayout;
    std::vector<SwDescriptorSet> mPrefilterMipDescriptorSets;  // one per prefilter mip level

    // The set-1 descriptor set bound during the geometry passes.
    SwDescriptorSet mConsumeDescriptorSet;
};

class System : public SwSystem {
private:
    Resources mResources;
    std::uint32_t mPrefilterMipLevels{0};
    float mIblIntensity{1.f};

    void initializeResources() override;
    void initializePasses() override;  // no per-frame pass; bakes run as one-shot immediate submits

public:
    // Set-1 layout, referenced by SwMaterial's geometry pipeline layouts. Created before SwMaterial::init,
    // independently of any System instance (mirrors SwMaterialResources::sMaterialResourcesDescriptorLayout).
    static SwDescriptorLayout sConsumeDescriptorLayout;
    static void init();
    static void cleanup();

    System(SwScene& scene);

    // Reconvolve the irradiance + specular-prefilter maps from a freshly-loaded environment equirect.
    // Called whenever the skybox changes.
    void bakeFromEnvironment(vk::ImageView environmentView, vk::Sampler environmentSampler);

    inline SwDescriptorSet& getConsumeDescriptorSet() { return mResources.mConsumeDescriptorSet; }
    inline float getMaxPrefilterMip() const { return mPrefilterMipLevels > 0 ? static_cast<float>(mPrefilterMipLevels - 1) : 0.f; }
    inline float getIblIntensity() const { return mIblIntensity; }
    inline float* getIblIntensityPtr() { return &mIblIntensity; }
    inline Resources& getResources() { return mResources; }
};

}  // namespace SwIBL
