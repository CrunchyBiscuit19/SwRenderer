#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwPushConstant.h>
#include <Resource/SwSampler.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>

class SwScene;

namespace SwIBL {
static const std::filesystem::path IRRADIANCE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwIBLIrradiance.comp.spv"};
static const std::filesystem::path PREFILTER_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwIBLPrefilter.comp.spv"};
static const std::filesystem::path BRDF_LUT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwIBLBrdfLut.comp.spv"};

// Skybox draw: rasterizes the environment equirect behind the scene geometry.
constexpr std::uint32_t NUM_SKYBOX_VERTICES{36};
static const std::filesystem::path SKYBOX_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwIBLSkybox.vert.spv"};
static const std::filesystem::path SKYBOX_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwIBLSkybox.frag.spv"};
static const std::filesystem::path SKYBOX_DEFAULT_HDR_PATH{std::filesystem::path(SKYBOXES_PATH) / "AutumnHillView2k.hdr"};

// HDR float maps so the prefilter/irradiance integrals stay meaningful (the environment is HDR).
constexpr vk::Format IBL_FORMAT{vk::Format::eR16G16B16A16Sfloat};
constexpr vk::Format BRDF_LUT_FORMAT{vk::Format::eR16G16Sfloat};

constexpr vk::Extent3D IRRADIANCE_EXTENT{64, 32, 1};
constexpr vk::Extent3D PREFILTER_EXTENT{128, 64, 1};  // mip-chained; mip level encodes roughness
constexpr vk::Extent3D BRDF_LUT_EXTENT{512, 512, 1};

// IBL ambient component mask passed to the geometry shaders (mirrors SwGeometry's IBL_DIFFUSE / IBL_SPECULAR).
constexpr std::uint32_t IBL_DIFFUSE{1u};
constexpr std::uint32_t IBL_SPECULAR{2u};

// Set-1 bindings consumed by the geometry/transparent fragment shaders.
constexpr std::uint32_t CONSUME_IRRADIANCE_BINDING{0};
constexpr std::uint32_t CONSUME_PREFILTER_BINDING{1};
constexpr std::uint32_t CONSUME_BRDF_LUT_BINDING{2};

struct PrefilterPC : SwPC<PrefilterPC> {
    float mRoughness;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct DrawPC : SwPC<DrawPC> {
    vk::DeviceAddress mDrawVertexBuffer;
    vk::DeviceAddress mPerFrameBuffer;

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    static SwDescriptorLayout sConsumeDescriptorLayout;

    // Baked maps. Irradiance + BRDF LUT are single-level; prefilter is mip-chained (roughness per mip).
    SwColorImage2D mIrradianceImage;
    SwColorImage2D mPrefilterImage;
    SwColorImage2D mBrdfLutImage;

    SwSampler mEnvSampler;  // equirect maps: repeat longitude (U), clamp latitude (V), trilinear for the prefilter mips
    SwSampler mLutSampler;  // BRDF LUT: clamp both axes

    SwComputePipelineBundle mBrdfLutPipelineBundle;
    SwPipelineLayout mBrdfLutPipelineLayout;
    SwDescriptorLayout mBrdfLutDescriptorLayout;
    SwDescriptorSet mBrdfLutDescriptorSet;

    SwDescriptorLayout mBakeInputDescriptorLayout;

    SwComputePipelineBundle mIrradiancePipelineBundle;
    SwPipelineLayout mIrradiancePipelineLayout;
    SwDescriptorSet mIrradianceDescriptorSet;

    SwComputePipelineBundle mPrefilterPipelineBundle;
    SwPipelineLayout mPrefilterPipelineLayout;
    std::vector<SwDescriptorSet> mPrefilterMipDescriptorSets;  

    SwDescriptorSet mConsumeDescriptorSet;

    SwColorImage2D mDrawImage;

    SwSampler mDrawSampler;

    const std::vector<float> mDrawVertices = {
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
    SwAllocatedBuffer mDrawVertexBuffer;

    SwGraphicsPipelineBundle mDrawPipelineBundle;
    SwPipelineLayout mDrawPipelineLayout;

    SwDescriptorSet mDrawDescriptorSet;
    SwDescriptorLayout mDrawDescriptorLayout;

    DrawPC mDrawPushConstants;

    static void init();
    static void cleanup();
};

class System : public SwSystem {
private:
    Resources mResources;
    std::uint32_t mPrefilterMipLevels{0};
    float mIblIntensity{1.f};
    std::uint32_t mIblComponents{IBL_DIFFUSE | IBL_SPECULAR};  // which ambient terms to apply (GUI-controlled)
    // Cosine-weighted average luminance of the loaded environment. The IBL ambient is divided by this so a
    // bright HDR does not flood surfaces: IBL Intensity then scales a unit-mean fill, independent of the
    // environment's absolute radiance. Defaults to 1 (no normalization) until an environment is loaded.
    float mEnvAvgLuminance{1.f};
    std::optional<std::filesystem::path> mLoadFromFile{std::nullopt};
    bool mActive{true};

    void initializeResources() override;
    void initializePasses() override;  // skybox draw pass; the bakes run as one-shot immediate submits
    void initializePushConstants() override;

    // Reconvolve the irradiance + specular-prefilter maps from a freshly-loaded environment equirect.
    // Called whenever the environment changes.
    void bakeFromEnvironment(vk::ImageView environmentView, vk::Sampler environmentSampler);

public:
    System(SwScene& scene);

    // Load a new environment equirect, repoint the skybox draw, and rebake the IBL maps.
    void reinitializeOnUpdate(std::optional<std::filesystem::path>);

    void refreshPushConstants() override;

    inline void toggleActive() { mActive = !mActive; }
    inline bool isActive() const { return mActive; }
    inline bool isFileSelected() const { return mLoadFromFile.has_value(); }

    inline SwDescriptorSet& getConsumeDescriptorSet() { return mResources.mConsumeDescriptorSet; }
    inline float getMaxPrefilterMip() const { return mPrefilterMipLevels > 0 ? static_cast<float>(mPrefilterMipLevels - 1) : 0.f; }
    inline float getIblIntensity() const { return mIblIntensity; }
    inline float* getIblIntensityPtr() { return &mIblIntensity; }
    inline std::uint32_t getIblComponents() const { return mIblComponents; }
    inline std::uint32_t* getIblComponentsPtr() { return &mIblComponents; }
    inline float getEnvAvgLuminance() const { return mEnvAvgLuminance; }
    inline Resources& getResources() { return mResources; }
};

}  // namespace SwIBL
