#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>

#include <cstdint>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <filesystem>

class SwDescriptorLayout;
struct SwRendererContext;
class SwShader;

class SwMaterialTexture {
private:
    SwColorImage2D& mImage;
    SwSampler& mSampler;

public:
    SwMaterialTexture(SwColorImage2D& image, SwSampler& sampler);

    inline SwColorImage2D& getImage() { return mImage; }
    inline SwSampler& getSampler() { return mSampler; }

    SwMaterialTexture(SwMaterialTexture&&) noexcept = default;
    SwMaterialTexture& operator=(SwMaterialTexture&&) noexcept = default;

    SwMaterialTexture(const SwMaterialTexture&) = delete;
    SwMaterialTexture& operator=(const SwMaterialTexture&) = delete;
};

struct SwMaterialConstants {
private:
    static const std::uint32_t MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE{256 * 1024 * 1024};  // 256 MB

public:
    static SwStagingBuffer sMaterialConstantsStagingBuffer;

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
    static SwRendererContext sRendererContext;

public:
    static const std::uint32_t MAX_TEXTURE_ARRAY_SLOTS = 1 << 8;

    SwMaterialTexture mBase;
    SwMaterialTexture mMetallicRoughness;
    SwMaterialTexture mNormal;
    SwMaterialTexture mOcclusion;
    SwMaterialTexture mEmissive;

    static SwDescriptorLayout sMaterialResourcesDescriptorLayout;

    SwMaterialResources(SwMaterialTexture base, SwMaterialTexture metallicRoughness, SwMaterialTexture normal, SwMaterialTexture occlusion, SwMaterialTexture emissive);

    static void init(SwRendererContext rendererContext);

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
    static SwRendererContext sRendererContext;
    static std::uint32_t sLatestMaterialId;

    std::string mName;
    SwMaterialPipelineOptions mMaterialPipelineOptions;
    SwMaterialConstants mMaterialConstants;
    SwMaterialResources mMaterialResources;
    SwGraphicsPipelineBundle mPipelineBundle;

    static std::unordered_map<SwMaterialPipelineOptions, SwPipelinePipeline> sMaterialPipelines;
    static SwPipelineLayout sOpaquePipelineLayout;
    static SwPipelineLayout sTransparentPipelineLayout;
    static std::filesystem::path GEOMETRY_VERTEX_SHADER_PATH;
    static SwShader sVertexShader;
    static std::filesystem::path GEOMETRY_OPAQUE_FRAGMENT_SHADER_PATH;
    static SwShader sOpaqueFragmentShader;
    static std::filesystem::path GEOMETRY_TRANSPARENT_FRAGMENT_SHADER_PATH;
    static SwShader sTransparentFragmentShader;

    void constructMaterialPipeline(SwMaterialPipelineOptions materialPipelineOptions) const; 

public:
    static const std::uint32_t NUM_PBR_IMAGES = 5;

    std::uint32_t mRelativeMaterialIndex;

    SwMaterial(
        std::string name, std::uint32_t relativeMaterialIndex, SwMaterialPipelineOptions materialPipelineOptions, SwMaterialConstants materialConstants,
        SwMaterialResources materialResources
    );

    static void init(SwRendererContext rendererContext);
    static void cleanup();

    inline SwGraphicsPipelineBundle getPipelineBundle() { return mPipelineBundle; }

    inline fastgltf::AlphaMode getAlphaMode() { return mMaterialPipelineOptions.alphaMode; }

    inline SwMaterialResources& getResources() { return mMaterialResources; }
};