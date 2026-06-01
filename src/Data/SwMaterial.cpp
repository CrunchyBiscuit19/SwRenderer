#include <Data/SwMaterial.h>
#include <Renderer/SwLogger.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwShader.h>
#include <Scene/SwGeometry.h>
#include <Scene/SwScene.h>
#include <quill/LogMacros.h>

SwMaterialTexture SwMaterialTexture::DEFAULT_WHITE_TEXTURE{nullptr, nullptr};
SwMaterialTexture SwMaterialTexture::DEFAULT_ERROR_TEXTURE{nullptr, nullptr};

SwMaterialTexture::SwMaterialTexture(SwColorImage2D* image, SwSampler* sampler) : mImage(image), mSampler(sampler) {}

SwStagingBuffer SwMaterialConstants::sMaterialConstantsStagingBuffer{};

void SwMaterialConstants::init() { sMaterialConstantsStagingBuffer = SwBufferFactory::createStagingBuffer(MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE); }

void SwMaterialConstants::cleanup() { sMaterialConstantsStagingBuffer.destroy(); }

SwRendererContext SwMaterialResources::sRendererContext{};
SwDescriptorLayout SwMaterialResources::sMaterialResourcesDescriptorLayout{};

SwMaterialResources::SwMaterialResources(
    SwMaterialTexture base, SwMaterialTexture metallicRoughness, SwMaterialTexture normal, SwMaterialTexture occlusion, SwMaterialTexture emissive
)
    : mBase(std::move(base)),
      mMetallicRoughness(std::move(metallicRoughness)),
      mNormal(std::move(normal)),
      mOcclusion(std::move(occlusion)),
      mEmissive(std::move(emissive)) {}

void SwMaterialResources::init(SwRendererContext rendererContext) {
    sRendererContext = rendererContext;
    sMaterialResourcesDescriptorLayout = sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_ARRAY_SLOTS}}, vk::ShaderStageFlagBits::eVertex |    vk::ShaderStageFlagBits::eFragment, true
    );
    SwMaterialTexture::DEFAULT_WHITE_TEXTURE = SwMaterialTexture(&SwImageFactory::sDefaultImages[SwImageFactory::SwDefaultImageOption::White], &SwSampler::sDefaultSampler);
    SwMaterialTexture::DEFAULT_ERROR_TEXTURE = SwMaterialTexture(&SwImageFactory::sDefaultImages[SwImageFactory::SwDefaultImageOption::White], &SwSampler::sDefaultSampler);
};

void SwMaterialResources::cleanup() {
    sMaterialResourcesDescriptorLayout.destroy();
}

SwRendererContext SwMaterial::sRendererContext{};
std::uint32_t SwMaterial::sLatestMaterialId{0};
std::unordered_map<SwMaterialPipelineOptions, SwGraphicsPipelineBundle> SwMaterial::sMaterialPipelineBundles{};
SwPipelineLayout SwMaterial::sOpaquePipelineLayout;
SwPipelineLayout SwMaterial::sTransparentPipelineLayout;
const std::filesystem::path SwMaterial::GEOMETRY_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwGeometryWork.vert.spv"};
SwShader SwMaterial::sVertexShader;
const std::filesystem::path SwMaterial::GEOMETRY_OPAQUE_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "SwGeometryWorkOpaque.frag.spv"};
SwShader SwMaterial::sOpaqueFragmentShader;
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

void SwMaterial::init(SwRendererContext rendererContext) {
    sRendererContext = rendererContext;

    sOpaquePipelineLayout =
        SwPipelineFactory::createPipelineLayout(SwMaterialResources::sMaterialResourcesDescriptorLayout.getRawLayout(), SwGeometry::WorkPC::getRange());
    sTransparentPipelineLayout =
        SwPipelineFactory::createPipelineLayout(SwMaterialResources::sMaterialResourcesDescriptorLayout.getRawLayout(), SwGeometry::WorkPC::getRange());

    sVertexShader = SwShaderFactory::createShader(GEOMETRY_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    sOpaqueFragmentShader = SwShaderFactory::createShader(GEOMETRY_OPAQUE_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);
    sTransparentFragmentShader = SwShaderFactory::createShader(GEOMETRY_TRANSPARENT_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);
}

void SwMaterial::constructMaterialPipeline(SwMaterialPipelineOptions materialPipelineOptions) const {
    vk::CullModeFlags cullMode = materialPipelineOptions.doubleSided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack;
    bool opaque = (materialPipelineOptions.alphaMode != fastgltf::AlphaMode::Blend);

    SwShader& vertexShader = sVertexShader;
    SwShader& fragShader = opaque ? sOpaqueFragmentShader : sTransparentFragmentShader;

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
    graphicsPipelineOptions.mVertexShader = vertexShader.getRawModule();
    graphicsPipelineOptions.mFragmentShader = fragShader.getRawModule();
    graphicsPipelineOptions.mLayout = opaque ? sOpaquePipelineLayout.getRawLayout() : sTransparentPipelineLayout.getRawLayout();
    graphicsPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    graphicsPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    graphicsPipelineOptions.mCullMode = cullMode;
    graphicsPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    graphicsPipelineOptions.mMultisamplingEnabled = false;
    graphicsPipelineOptions.mSampleShadingEnabled = false;
    if (opaque) {
        graphicsPipelineOptions.mColorAttachments =
            std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{{SwSwapchain::DRAW_FORMAT, noBlendState}};
    } else {
        graphicsPipelineOptions.mColorAttachments = std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{
            {SwSwapchain::DRAW_FORMAT, accumBlendState},
            {SwWBOIT::RVL_FORMAT, rvlBlendState},
        };
    }
    graphicsPipelineOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    graphicsPipelineOptions.mDepthTestEnabled = true;
    graphicsPipelineOptions.mDepthWriteEnabled = opaque;
    graphicsPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;

    auto [it, _] =
        sMaterialPipelineBundles.try_emplace(materialPipelineOptions, std::move(SwGraphicsPipelineFactory::createGraphicsPipeline(graphicsPipelineOptions)));
}

void SwMaterial::cleanup() {
    sTransparentFragmentShader.destroy();
    sOpaqueFragmentShader.destroy();
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
