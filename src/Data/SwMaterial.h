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
struct SwMaterialResourcesContext;
class SwShader;

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
    static const std::uint32_t MAX_TEXTURE_ARRAY_SLOTS = 1 << 10;

    static SwMaterialResourcesContext sMaterialResourcesContext;

public:
    SwMaterialImage mBase;
    SwMaterialImage mMetallicRoughness;
    SwMaterialImage mNormal;
    SwMaterialImage mOcclusion;
    SwMaterialImage mEmissive;

    static SwDescriptorLayout sMaterialResourcesDescriptorLayout;

    SwMaterialResources(SwMaterialImage base, SwMaterialImage metallicRoughness, SwMaterialImage normal, SwMaterialImage occlusion, SwMaterialImage emissive);

    static void init(SwMaterialResourcesContext materialResourcesContext);

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
    std::uint32_t mRelativeMaterialIndex;
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
    SwMaterial(
        std::string name, std::uint32_t relativeMaterialIndex, SwMaterialPipelineOptions materialPipelineOptions, SwMaterialConstants materialConstants,
        SwMaterialResources materialResources
    );

    static void init();
    static void cleanup();

    inline SwGraphicsPipelineBundle getPipelineBundle() { return mPipelineBundle; }

    inline fastgltf::AlphaMode getAlphaMode() { return mMaterialPipelineOptions.alphaMode; }
};