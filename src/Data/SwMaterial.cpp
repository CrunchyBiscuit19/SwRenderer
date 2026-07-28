#include <Data/SwMaterial.h>
#include <Renderer/SwLogger.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwGeometry.h>
#include <System/SwIBL.h>
#include <System/SwLighting.h>
#include <quill/LogMacros.h>

#include <format>
#include <magic_enum.hpp>

SwMaterialTexture SwMaterialTexture::sDefaultWhiteTexture{nullptr, nullptr};
SwMaterialTexture SwMaterialTexture::sDefaultErrorTexture{nullptr, nullptr};
SwMaterialTexture SwMaterialTexture::sDefaultFlatNormalTexture{nullptr, nullptr};

SwMaterialTexture::SwMaterialTexture(SwColorImage2D* image, SwSampler* sampler) : mImage(image), mSampler(sampler) {}

SwMaterialTexture SwMaterialTexture::retrieveDefaultWhiteTexture() { return SwMaterialTexture(sDefaultWhiteTexture.mImage, sDefaultWhiteTexture.mSampler); }

SwMaterialTexture SwMaterialTexture::retrieveDefaultErrorTexture() { return SwMaterialTexture(sDefaultErrorTexture.mImage, sDefaultErrorTexture.mSampler); }

SwMaterialTexture SwMaterialTexture::retrieveDefaultFlatNormalTexture() {
    return SwMaterialTexture(sDefaultFlatNormalTexture.mImage, sDefaultFlatNormalTexture.mSampler);
}

SwDescriptorLayout SwMaterialResources::sMaterialSamplersDescriptorLayout{};
SwDescriptorLayout SwMaterialResources::sMaterialTexturesDescriptorLayout{};

SwMaterialResources::SwMaterialResources(
    SwMaterialTexture base, SwMaterialTexture metallicRoughness, SwMaterialTexture normal, SwMaterialTexture occlusion, SwMaterialTexture emissive
)
    : mBase(std::move(base)),
      mMetallicRoughness(std::move(metallicRoughness)),
      mNormal(std::move(normal)),
      mOcclusion(std::move(occlusion)),
      mEmissive(std::move(emissive)) {}

void SwMaterialResources::init() {
    sMaterialSamplersDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "MaterialSamplersDescriptorSetLayout",
        {{0, vk::DescriptorType::eSampler, SwScene::NUM_MATERIALS * SwMaterial::NUM_PBR_IMAGES}},
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        true
    );
    sMaterialTexturesDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "MaterialTexturesDescriptorSetLayout",
        {{0, vk::DescriptorType::eSampledImage, SwScene::NUM_MATERIALS * SwMaterial::NUM_PBR_IMAGES}},
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        true
    );
    SwMaterialTexture::sDefaultWhiteTexture =
        SwMaterialTexture(&SwImageFactory::sDefaultImages[SwImageFactory::SwDefaultImageOption::White], &SwSampler::sDefaultSampler);
    SwMaterialTexture::sDefaultErrorTexture =
        SwMaterialTexture(&SwImageFactory::sDefaultImages[SwImageFactory::SwDefaultImageOption::Checkerboard], &SwSampler::sDefaultSampler);
    SwMaterialTexture::sDefaultFlatNormalTexture =
        SwMaterialTexture(&SwImageFactory::sDefaultImages[SwImageFactory::SwDefaultImageOption::FlatNormal], &SwSampler::sDefaultSampler);
};

void SwMaterialResources::cleanup() {
    sMaterialSamplersDescriptorLayout.destroy();
    sMaterialTexturesDescriptorLayout.destroy();
}

std::uint32_t SwMaterial::sLatestMaterialId{0};
std::unordered_map<SwMaterialPipelineOptions, std::uint32_t> SwMaterial::sMaterialPipelinesCreated{};
std::unordered_map<std::uint32_t, SwGraphicsPipelineBundle> SwMaterial::sMaterialPipelineBundles{};
SwPipelineLayout SwMaterial::sOpaquePipelineLayout;
SwPipelineLayout SwMaterial::sTransparentPipelineLayout;
static constexpr std::string_view SHADERS_PATH{SHADERS_DIR "/Geometry"};
const std::filesystem::path SwMaterial::VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Geometry.vert.spv"};
SwShader SwMaterial::sVertexShader;
const std::filesystem::path SwMaterial::OPAQUE_MASKED_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "OpaqueMasked.frag.spv"};
SwShader SwMaterial::sOpaqueMaskedFragmentShader;
const std::filesystem::path SwMaterial::TRANSPARENT_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Transparent.frag.spv"};
SwShader SwMaterial::sTransparentFragmentShader;

SwMaterial::SwMaterial(
    std::string name, std::uint32_t relativeMaterialIndex, SwMaterialPipelineOptions materialPipelineOptions, SwMaterial::Constant materialConstants,
    SwMaterialResources materialResources
)
    : mName(std::move(name)),
      mRelativeMaterialIndex(relativeMaterialIndex),
      mMaterialPipelineOptions(materialPipelineOptions),
      mMaterialConstants(materialConstants),
      mMaterialResources(std::move(materialResources)) {
    if (auto it = sMaterialPipelinesCreated.find(materialPipelineOptions); it != sMaterialPipelinesCreated.end()) {
        mPipelineId = it->second;
        return;
    }

    mPipelineId = constructMaterialPipeline(materialPipelineOptions);

    sLatestMaterialId++;
}

void SwMaterial::init() {
    sOpaquePipelineLayout =
        SwPipelineFactory::createPipelineLayout("GeometryOpaquePipelineLayout", SwGeometry::Resources::sGeometrySetLayouts, SwGeometry::DrawPC::getRange());
    sTransparentPipelineLayout = SwPipelineFactory::createPipelineLayout(
        "GeometryTransparentPipelineLayout", SwGeometry::Resources::sGeometrySetLayouts, SwGeometry::DrawPC::getRange()
    );

    sVertexShader = SwShaderFactory::createShader("GeometryVertexShaderModule", VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    sOpaqueMaskedFragmentShader =
        SwShaderFactory::createShader("GeometryOpaqueMaskedFragmentShaderModule", OPAQUE_MASKED_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);
    sTransparentFragmentShader =
        SwShaderFactory::createShader("GeometryTransparentFragmentShaderModule", TRANSPARENT_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);
}

vk::ShaderModule SwMaterial::getGeometryVertexShaderModule() { return sVertexShader.getHandle(); }

vk::ShaderModule SwMaterial::getOpaqueMaskedFragmentShaderModule() { return sOpaqueMaskedFragmentShader.getHandle(); }

std::uint32_t SwMaterial::constructMaterialPipeline(SwMaterialPipelineOptions materialPipelineOptions) const {
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
    graphicsPipelineOptions.mVertexShader = sVertexShader.getHandle();
    graphicsPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    graphicsPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    graphicsPipelineOptions.mCullMode = cullMode;
    graphicsPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    graphicsPipelineOptions.mMultisamplingEnabled = false;
    graphicsPipelineOptions.mSampleShadingEnabled = false;
    graphicsPipelineOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    graphicsPipelineOptions.mDepthTestEnabled = true;
    switch (materialPipelineOptions.alphaMode) {
        case AlphaMode::Opaque:
        case AlphaMode::Mask:
            // Masked entry point adds the alpha-cutout discard while the opaque entry point keeps [earlydepthstencil].
            graphicsPipelineOptions.mFragmentShader = sOpaqueMaskedFragmentShader.getHandle();
            graphicsPipelineOptions.mFragmentEntryPoint =
                materialPipelineOptions.alphaMode == AlphaMode::Mask ? std::string(MASKED_ENTRY_POINT) : std::string(OPAQUE_ENTRY_POINT);
            graphicsPipelineOptions.mLayout = sOpaquePipelineLayout.getHandle();
            graphicsPipelineOptions.mColorAttachments =
                std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{{SwSwapchain::DRAW_FORMAT, noBlendState}};
            graphicsPipelineOptions.mDepthWriteEnabled = true;
            graphicsPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
            break;
        case AlphaMode::Blend:
            // Tests against pre-pass depth for occlusion, never writes depth.
            graphicsPipelineOptions.mFragmentShader = sTransparentFragmentShader.getHandle();
            graphicsPipelineOptions.mLayout = sTransparentPipelineLayout.getHandle();
            graphicsPipelineOptions.mColorAttachments = std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{
                {SwSwapchain::DRAW_FORMAT, accumBlendState},
                {SwWBOIT::RVL_FORMAT, rvlBlendState},
            };
            graphicsPipelineOptions.mDepthWriteEnabled = false;
            graphicsPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
            break;
    }

    const std::string pipelineName = std::format(
        "Geometry{}{}Pipeline", magic_enum::enum_name(materialPipelineOptions.alphaMode), materialPipelineOptions.doubleSided ? "DoubleSided" : "SingleSided"
    );
    SwGraphicsPipelineBundle bundle = SwGraphicsPipelineFactory::createGraphicsPipeline(pipelineName, graphicsPipelineOptions);
    const std::uint32_t pipelineId = bundle.getID();
    sMaterialPipelineBundles.try_emplace(pipelineId, std::move(bundle));
    sMaterialPipelinesCreated.emplace(materialPipelineOptions, pipelineId);
    return pipelineId;
}

void SwMaterial::cleanup() {
    sTransparentFragmentShader.destroy();
    sOpaqueMaskedFragmentShader.destroy();
    sVertexShader.destroy();
    sTransparentPipelineLayout.destroy();
    sOpaquePipelineLayout.destroy();
    sMaterialPipelineBundles.clear();
}

SwMaterial::Type SwMaterial::getMaterialTypeFromAlphaMode(AlphaMode alphaMode) {
    switch (alphaMode) {
        case AlphaMode::Opaque:
            return Type::Opaque;
            break;
        case AlphaMode::Mask:
            return Type::Mask;
            break;
        case AlphaMode::Blend:
            return Type::Transparent;
            break;
    }
    std::unreachable();
}
