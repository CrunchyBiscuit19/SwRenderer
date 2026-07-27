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
#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>

class SwScene;

namespace SwIBL {
static constexpr std::string_view SHADERS_PATH{SHADERS_DIR "/IBL"};
static const std::filesystem::path IRRADIANCE_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Irradiance.comp.spv"};
static const std::filesystem::path PREFILTER_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Prefilter.comp.spv"};
static const std::filesystem::path BRDF_LUT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "BrdfLut.comp.spv"};

constexpr std::uint32_t NUM_SKYBOX_VERTICES{36};
static const std::filesystem::path SKYBOX_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Skybox.vert.spv"};
static const std::filesystem::path SKYBOX_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Skybox.frag.spv"};
static const std::filesystem::path SKYBOX_DEFAULT_HDR_PATH{std::filesystem::path(SKYBOXES_PATH) / "AutumnHillView2k.hdr"};

constexpr vk::Format FORMAT{vk::Format::eR16G16B16A16Sfloat};
constexpr vk::Format BRDF_LUT_FORMAT{vk::Format::eR16G16Sfloat};

constexpr vk::Extent3D IRRADIANCE_EXTENT{64, 32, 1};
constexpr vk::Extent3D PREFILTER_EXTENT{128, 64, 1};
constexpr vk::Extent3D BRDF_LUT_EXTENT{512, 512, 1};

constexpr std::uint32_t MAX_PREFILTER_MIP_LEVELS{1 << 4};

enum class Component : std::uint32_t {
    Diffuse = 1u << 0,
    Specular = 1u << 1,
};
constexpr Component operator|(Component a, Component b) { return static_cast<Component>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b)); }
constexpr Component operator&(Component a, Component b) { return static_cast<Component>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)); }

// Bindings consumed by the geometry/transparent fragment shaders. Samplers keep the original binding
// slots, the sampled images follow directly after them.
constexpr std::uint32_t CONSUME_IRRADIANCE_SAMPLER_BINDING{0};
constexpr std::uint32_t CONSUME_PREFILTER_SAMPLER_BINDING{1};
constexpr std::uint32_t CONSUME_BRDF_LUT_SAMPLER_BINDING{2};
constexpr std::uint32_t CONSUME_IRRADIANCE_IMAGE_BINDING{3};
constexpr std::uint32_t CONSUME_PREFILTER_IMAGE_BINDING{4};
constexpr std::uint32_t CONSUME_BRDF_LUT_IMAGE_BINDING{5};

struct PrefilterPC : SwPC<PrefilterPC> {
    std::uint32_t mTotalMipLevels{0};
    glm::uvec2 mBaseExtent{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};

struct SkyboxPC : SwPC<SkyboxPC> {
    vk::DeviceAddress mSkyboxVertexBuffer{0};
    vk::DeviceAddress mFrameBuffer{0};

    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eVertex;
};

struct Resources {
    static SwDescriptorLayout sConsumeDescriptorLayout;

    SwColorImage2D mIrradianceImage;
    SwColorImage2D mPrefilterImage;
    SwColorImage2D mBrdfLutImage;

    SwSampler mEnvSampler;  // Equirect maps repeat longitude (U), clamp latitude (V), trilinear for the prefilter mips
    SwSampler mLutSampler;  // BRDF LUT clamp both axes

    SwComputePipelineBundle mBrdfLutPipelineBundle;
    SwPipelineLayout mBrdfLutPipelineLayout;
    SwDescriptorLayout mBrdfLutDescriptorLayout;
    SwDescriptorSet mBrdfLutDescriptorSet;

    SwComputePipelineBundle mIrradiancePipelineBundle;
    SwPipelineLayout mIrradiancePipelineLayout;
    SwDescriptorLayout mIrradianceDescriptorLayout;
    SwDescriptorSet mIrradianceDescriptorSet;

    PrefilterPC mPrefilterPushConstants;
    SwComputePipelineBundle mPrefilterPipelineBundle;
    SwPipelineLayout mPrefilterPipelineLayout;
    SwDescriptorLayout mPrefilterDescriptorLayout;
    SwDescriptorSet mPrefilterMipDescriptorSet;

    SwDescriptorSet mConsumeDescriptorSet;

    SwColorImage2D mSkyboxImage;
    SwSampler mSkyboxSampler;

    const std::vector<float> mSkyboxVertices = {
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
    SwAllocatedBuffer mSkyboxVertexBuffer;

    SwGraphicsPipelineBundle mSkyboxPipelineBundle;
    SwPipelineLayout mSkyboxPipelineLayout;

    SwDescriptorSet mSkyboxDescriptorSet;
    SwDescriptorLayout mSkyboxDescriptorLayout;

    SkyboxPC mSkyboxPushConstants;

    static void init();
    static void cleanup();
};

class System : public SwSystem {
private:
    Resources mResources;
    std::uint32_t mPrefilterMipLevels{0};
    float mIblIntensity{1.f};
    Component mIblComponents{Component::Diffuse | Component::Specular};  // which ambient terms to apply (GUI-controlled)
    // Cosine-weighted average luminance of the loaded environment. The IBL ambient is divided by this so a bright HDR does not flood surfaces. 
    float mEnvAvgLuminance{1.f};
    std::optional<std::filesystem::path> mLoadFromFile{std::nullopt};
    bool mActive{true};

    void initializeResources() override;
    void initializePasses() override;  // skybox draw pass; the bakes run as one-shot immediate submits
    void refreshDataUsage() override;

    // Reconvolve the irradiance + specular-prefilter maps from a freshly-loaded environment equirect.
    void bakeFromEnvironment(SwImage& environment, vk::Sampler environmentSampler);

public:
    System(SwScene& scene);

    // Load a new environment equirect, repoint the skybox draw, and rebake the IBL maps.
    void reinitializeOnUpdate(std::optional<std::filesystem::path>);

    inline void toggleActive() { mActive = !mActive; }
    inline bool isActive() const { return mActive; }
    inline bool isFileSelected() const { return mLoadFromFile.has_value(); }

    inline SwDescriptorSet& getConsumeDescriptorSet() { return mResources.mConsumeDescriptorSet; }
    inline float getMaxPrefilterMip() const { return mPrefilterMipLevels > 0 ? static_cast<float>(mPrefilterMipLevels - 1) : 0.f; }
    inline float getIblIntensity() const { return mIblIntensity; }
    inline float* getIblIntensityPtr() { return &mIblIntensity; }
    inline Component getIblComponents() const { return mIblComponents; }
    inline Component* getIblComponentsPtr() { return &mIblComponents; }
    inline float getEnvAvgLuminance() const { return mEnvAvgLuminance; }
    inline Resources& getResources() { return mResources; }
};

}  // namespace SwIBL
