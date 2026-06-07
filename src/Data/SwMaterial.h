#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>

#include <cstdint>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <filesystem>

class SwDescriptorLayout;
struct SwRendererContext;
class SwShader;

class SwMaterialTexture {
private:
    SwColorImage2D* mImage;
    SwSampler* mSampler;

public:
    enum class Type { Base, MetallicRoughness, Normal, Occlusion, Emissive };

    static SwMaterialTexture sDefaultWhiteTexture;
    static SwMaterialTexture sDefaultErrorTexture;

    static constexpr vk::Format SRGB_IMAGE_FORMAT{vk::Format::eR8G8B8A8Srgb};
    static constexpr vk::Format UNORM_IMAGE_FORMAT{vk::Format::eR8G8B8A8Unorm};

    SwMaterialTexture(SwColorImage2D* image, SwSampler* sampler);

    inline SwColorImage2D& getImage() { return *mImage; }
    inline SwSampler& getSampler() { return *mSampler; }

    SwMaterialTexture(SwMaterialTexture&&) noexcept = default;
    SwMaterialTexture& operator=(SwMaterialTexture&&) noexcept = default;

    SwMaterialTexture(const SwMaterialTexture&) = delete;
    SwMaterialTexture& operator=(const SwMaterialTexture&) = delete;

    static SwMaterialTexture retrieveDefaultWhiteTexture();
    static SwMaterialTexture retrieveDefaultErrorTexture();
};

struct SwMaterialConstants {
private:
    static constexpr std::uint32_t MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE{1 << 20};  // 1 MB

public:
    static SwStagingBuffer sMaterialConstantsStaging;

    glm::vec4 mBaseFactor;
    glm::vec4 mEmissiveFactor;
    glm::vec2 mMetallicRoughnessFactor;
    float mNormalScale;
    float mOcclusionStrength;

    static void init();
    static void cleanup();
};

struct SwMaterialResources {
private:

public:
    SwMaterialTexture mBase;
    SwMaterialTexture mMetallicRoughness;
    SwMaterialTexture mNormal;
    SwMaterialTexture mOcclusion;
    SwMaterialTexture mEmissive;

    static SwDescriptorLayout sMaterialResourcesDescriptorLayout;

    SwMaterialResources(SwMaterialTexture base, SwMaterialTexture metallicRoughness, SwMaterialTexture normal, SwMaterialTexture occlusion, SwMaterialTexture emissive);

    static void init();

    static void cleanup();
};

struct SwMaterialPipelineOptions {
    bool doubleSided;
    fastgltf::AlphaMode alphaMode;
    bool operator==(const SwMaterialPipelineOptions& other) const { return (doubleSided == other.doubleSided && alphaMode == other.alphaMode); }
};

template <>
struct std::hash<SwMaterialPipelineOptions> {
    // Compute individual hash values for strings
    // Combine them using XOR and bit shifting
    std::size_t operator()(const SwMaterialPipelineOptions& k) const {
        return ((std::hash<bool>()(k.doubleSided) ^ (std::hash<fastgltf::AlphaMode>()(k.alphaMode) << 1)) >> 1);
    }
};

class SwMaterial {
private:
    static std::uint32_t sLatestMaterialId;

    std::string mName;
    SwMaterialPipelineOptions mMaterialPipelineOptions;
    SwMaterialConstants mMaterialConstants;
    SwMaterialResources mMaterialResources;
    SwGraphicsPipelineBundle* mMaterialPipelineBundle{nullptr};

    static std::unordered_map<SwMaterialPipelineOptions, SwGraphicsPipelineBundle> sMaterialPipelineBundles;
    static SwPipelineLayout sOpaquePipelineLayout;
    static SwPipelineLayout sTransparentPipelineLayout;
    static const std::filesystem::path GEOMETRY_VERTEX_SHADER_PATH;
    static SwShader sVertexShader;
    static const std::filesystem::path GEOMETRY_OPAQUE_MASKED_FRAGMENT_SHADER_PATH;
    static SwShader sOpaqueMaskedFragmentShader;
    static constexpr std::string_view GEOMETRY_OPAQUE_ENTRY_POINT{"mainOpaque"};
    static constexpr std::string_view GEOMETRY_MASKED_ENTRY_POINT{"mainMasked"};
    static const std::filesystem::path GEOMETRY_TRANSPARENT_FRAGMENT_SHADER_PATH;
    static SwShader sTransparentFragmentShader;

    void constructMaterialPipeline(SwMaterialPipelineOptions materialPipelineOptions) const; 

public:
    enum class Type { Opaque, Mask, Transparent };

    static constexpr std::uint32_t NUM_PBR_IMAGES{5};

    std::uint32_t mRelativeMaterialIndex;

    SwMaterial(
        std::string name, std::uint32_t relativeMaterialIndex, SwMaterialPipelineOptions materialPipelineOptions, SwMaterialConstants materialConstants,
        SwMaterialResources materialResources
    );

    static void init();
    static void cleanup();

    static Type getMaterialTypeFromAlphaMode(fastgltf::AlphaMode alphaMode);

    inline SwGraphicsPipelineBundle& getPipelineBundle() { return *mMaterialPipelineBundle; }

    inline fastgltf::AlphaMode getAlphaMode() { return mMaterialPipelineOptions.alphaMode; }

    inline bool isDoubleSided() { return mMaterialPipelineOptions.doubleSided; }

    inline SwMaterialResources& getResources() { return mMaterialResources; }
};