#include <Data/SwMaterial.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwLogger.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwGeometry.h>
#include <Scene/SwScene.h>
#include <fmt/core.h>
#include <magic_enum.hpp>
#include <quill/LogMacros.h>

SwMaterialTexture SwMaterialTexture::sDefaultWhiteTexture{nullptr, nullptr};
SwMaterialTexture SwMaterialTexture::sDefaultErrorTexture{nullptr, nullptr};

SwMaterialTexture::SwMaterialTexture(SwColorImage2D* image, SwSampler* sampler) : mImage(image), mSampler(sampler) {}

SwMaterialTexture SwMaterialTexture::retrieveDefaultWhiteTexture() { return SwMaterialTexture(sDefaultWhiteTexture.mImage, sDefaultWhiteTexture.mSampler); }

SwMaterialTexture SwMaterialTexture::retrieveDefaultErrorTexture() { return SwMaterialTexture(sDefaultErrorTexture.mImage, sDefaultErrorTexture.mSampler); }

SwStagingBuffer SwMaterialConstants::sMaterialConstantsStaging{};

void SwMaterialConstants::init() { sMaterialConstantsStaging = SwBufferFactory::createStagingBuffer("MaterialConstantsStagingBuffer", MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE); }

void SwMaterialConstants::cleanup() { sMaterialConstantsStaging.destroy(); }

SwDescriptorLayout SwMaterialResources::sMaterialResourcesDescriptorLayout{};

SwMaterialResources::SwMaterialResources(
    SwMaterialTexture base, SwMaterialTexture metallicRoughness, SwMaterialTexture normal, SwMaterialTexture occlusion, SwMaterialTexture emissive
)
    : mBase(std::move(base)),
      mMetallicRoughness(std::move(metallicRoughness)),
      mNormal(std::move(normal)),
      mOcclusion(std::move(occlusion)),
      mEmissive(std::move(emissive)) {}

void SwMaterialResources::init() {
    sMaterialResourcesDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "MaterialResourcesDescriptorSetLayout",
        {{0, vk::DescriptorType::eCombinedImageSampler, SwScene::SCENE_INITIAL_NUM_MATERIALS * SwMaterial::NUM_PBR_IMAGES}},
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        true
    );
    SwMaterialTexture::sDefaultWhiteTexture =
        SwMaterialTexture(&SwImageFactory::sDefaultImages[SwImageFactory::SwDefaultImageOption::White], &SwSampler::sDefaultSampler);
    SwMaterialTexture::sDefaultErrorTexture =
        SwMaterialTexture(&SwImageFactory::sDefaultImages[SwImageFactory::SwDefaultImageOption::Checkerboard], &SwSampler::sDefaultSampler);
};

void SwMaterialResources::cleanup() { sMaterialResourcesDescriptorLayout.destroy(); }

std::uint32_t SwMaterial::sLatestMaterialId{0};
std::unordered_map<SwMaterialPipelineOptions, SwGraphicsPipelineBundle> SwMaterial::sMaterialPipelineBundles{};
SwPipelineLayout SwMaterial::sOpaquePipelineLayout;
SwPipelineLayout SwMaterial::sTransparentPipelineLayout;
const std::filesystem::path SwMaterial::GEOMETRY_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwGeometry.vert.spv"};
SwShader SwMaterial::sVertexShader;
const std::filesystem::path SwMaterial::GEOMETRY_OPAQUE_MASKED_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwGeometryWorkOpaqueMasked.frag.spv"};
SwShader SwMaterial::sOpaqueMaskedFragmentShader;
const std::filesystem::path SwMaterial::GEOMETRY_TRANSPARENT_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwGeometryWorkTransparent.frag.spv"};
SwShader SwMaterial::sTransparentFragmentShader;

SwMaterial::SwMaterial(
    std::string name, std::uint32_t relativeMaterialIndex, SwMaterialPipelineOptions materialPipelineOptions, SwMaterialConstants materialConstants,
    SwMaterialResources materialResources
)
    : mName(std::move(name)),
      mRelativeMaterialIndex(relativeMaterialIndex),
      mMaterialPipelineOptions(materialPipelineOptions),
      mMaterialConstants(materialConstants),
      mMaterialResources(std::move(materialResources)) {
    if (auto it = sMaterialPipelineBundles.find(materialPipelineOptions); it != sMaterialPipelineBundles.end()) {
        mMaterialPipelineBundle = &it->second;
        return;
    }

    constructMaterialPipeline(materialPipelineOptions);

    mMaterialPipelineBundle = &sMaterialPipelineBundles[materialPipelineOptions];

    sLatestMaterialId++;
}

void SwMaterial::init() {

    sOpaquePipelineLayout = SwPipelineFactory::createPipelineLayout(
        "GeometryOpaquePipelineLayout", SwMaterialResources::sMaterialResourcesDescriptorLayout.getRawLayout(), SwGeometry::WorkPC::getRange()
    );
    sTransparentPipelineLayout = SwPipelineFactory::createPipelineLayout(
        "GeometryTransparentPipelineLayout", SwMaterialResources::sMaterialResourcesDescriptorLayout.getRawLayout(), SwGeometry::WorkPC::getRange()
    );

    sVertexShader = SwShaderFactory::createShader("GeometryVertexShaderModule", GEOMETRY_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    sOpaqueMaskedFragmentShader =
        SwShaderFactory::createShader("GeometryOpaqueMaskedFragmentShaderModule", GEOMETRY_OPAQUE_MASKED_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);
    sTransparentFragmentShader =
        SwShaderFactory::createShader("GeometryTransparentFragmentShaderModule", GEOMETRY_TRANSPARENT_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);
}

void SwMaterial::constructMaterialPipeline(SwMaterialPipelineOptions materialPipelineOptions) const {
    vk::CullModeFlags cullMode = materialPipelineOptions.doubleSided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack;

    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    noBlendState.blendEnable = VK_FALSE;

    vk::PipelineColorBlendAttachmentState accumBlendState{};
    accumBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    accumBlendState.blendEnable = VK_TRUE;
    accumBlendState.srcColorBlendFactor = vk::BlendFactor::eOne;
    accumBlendState.dstColorBlendFactor = vk::BlendFactor::eOne;
    accumBlendState.colorBlendOp = vk::BlendOp::eAdd;
    accumBlendState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    accumBlendState.dstAlphaBlendFactor = vk::BlendFactor::eOne;
    accumBlendState.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendAttachmentState rvlBlendState{};
    rvlBlendState.colorWriteMask = vk::ColorComponentFlagBits::eR;
    rvlBlendState.blendEnable = VK_TRUE;
    rvlBlendState.srcColorBlendFactor = vk::BlendFactor::eZero;
    rvlBlendState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;
    rvlBlendState.colorBlendOp = vk::BlendOp::eAdd;
    rvlBlendState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    rvlBlendState.dstAlphaBlendFactor = vk::BlendFactor::eOne;
    rvlBlendState.alphaBlendOp = vk::BlendOp::eAdd;

    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions graphicsPipelineOptions;
    graphicsPipelineOptions.mVertexShader = sVertexShader.getRawModule();
    graphicsPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    graphicsPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    graphicsPipelineOptions.mCullMode = cullMode;
    graphicsPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    graphicsPipelineOptions.mMultisamplingEnabled = false;
    graphicsPipelineOptions.mSampleShadingEnabled = false;
    graphicsPipelineOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    graphicsPipelineOptions.mDepthTestEnabled = true;
    switch (materialPipelineOptions.alphaMode) {
        case fastgltf::AlphaMode::Opaque:
        case fastgltf::AlphaMode::Mask:
            // Write depth normally with Reverse-Z test. Both share one fragment module; the masked
            // entry point adds the alpha-cutout discard (and drops early depth-stencil) while the
            // opaque entry point keeps [earlydepthstencil].
            graphicsPipelineOptions.mFragmentShader = sOpaqueMaskedFragmentShader.getRawModule();
            graphicsPipelineOptions.mFragmentEntryPoint =
                materialPipelineOptions.alphaMode == fastgltf::AlphaMode::Mask ? std::string(GEOMETRY_MASKED_ENTRY_POINT) : std::string(GEOMETRY_OPAQUE_ENTRY_POINT);
            graphicsPipelineOptions.mLayout = sOpaquePipelineLayout.getRawLayout();
            graphicsPipelineOptions.mColorAttachments =
                std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{{SwSwapchain::DRAW_FORMAT, noBlendState}};
            graphicsPipelineOptions.mDepthWriteEnabled = true;
            graphicsPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
            break;
        case fastgltf::AlphaMode::Blend:
            // Tests against pre-pass depth for occlusion; never writes depth.
            graphicsPipelineOptions.mFragmentShader = sTransparentFragmentShader.getRawModule();
            graphicsPipelineOptions.mLayout = sTransparentPipelineLayout.getRawLayout();
            graphicsPipelineOptions.mColorAttachments = std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{
                {SwSwapchain::DRAW_FORMAT, accumBlendState},
                {SwWBOIT::RVL_FORMAT, rvlBlendState},
            };
            graphicsPipelineOptions.mDepthWriteEnabled = false;
            graphicsPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
            break;
    }

    const std::string pipelineName = fmt::format(
        "Geometry{}{}Pipeline", magic_enum::enum_name(materialPipelineOptions.alphaMode), materialPipelineOptions.doubleSided ? "DoubleSided" : "SingleSided"
    );
    auto [it, _] = sMaterialPipelineBundles.try_emplace(
        materialPipelineOptions, std::move(SwGraphicsPipelineFactory::createGraphicsPipeline(pipelineName, graphicsPipelineOptions))
    );
}

void SwMaterial::cleanup() {
    sTransparentFragmentShader.destroy();
    sOpaqueMaskedFragmentShader.destroy();
    sVertexShader.destroy();
    sTransparentPipelineLayout.destroy();
    sOpaquePipelineLayout.destroy();
    sMaterialPipelineBundles.clear();
}

SwMaterial::Type SwMaterial::getMaterialTypeFromAlphaMode(fastgltf::AlphaMode alphaMode) {
    switch (alphaMode) {
        case fastgltf::AlphaMode::Opaque:
            return Type::Opaque;
            break;
        case fastgltf::AlphaMode::Mask:
            return Type::Mask;
            break;
        case fastgltf::AlphaMode::Blend:
            return Type::Transparent;
            break;
    }
    std::unreachable();
}
