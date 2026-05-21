#include <Data/SwMaterial.h>
#include <Pass/SwGeometry.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwShader.h>

SwStagingBuffer SwMaterialConstants::sMaterialConstantsStagingBuffer{};

void SwMaterialConstants::init() { sMaterialConstantsStagingBuffer = SwBufferFactory::createStagingBuffer(MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE); }

void SwMaterialConstants::cleanup() { sMaterialConstantsStagingBuffer.destroy(); }

SwRendererContext SwMaterialResources::sRendererContext{};
SwDescriptorLayout SwMaterialResources::sMaterialResourcesDescriptorLayout{};

SwMaterialResources::SwMaterialResources(
    SwMaterialImage base, SwMaterialImage metallicRoughness, SwMaterialImage normal, SwMaterialImage occlusion, SwMaterialImage emissive
)
    : mBase(std::move(base)),
      mMetallicRoughness(std::move(metallicRoughness)),
      mNormal(std::move(normal)),
      mOcclusion(std::move(occlusion)),
      mEmissive(std::move(emissive)) {}

void SwMaterialResources::init(SwRendererContext materialResourcesContext) {
    sRendererContext = materialResourcesContext;
    sMaterialResourcesDescriptorLayout = sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_ARRAY_SLOTS}}, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, true
    );
};

void SwMaterialResources::cleanup() { sMaterialResourcesDescriptorLayout.destroy(); }

std::uint32_t SwMaterial::sLatestMaterialId{0};
std::unordered_map<SwMaterialPipelineOptions, SwPipelinePipeline> SwMaterial::sMaterialPipelines{};
SwPipelineLayout SwMaterial::sOpaquePipelineLayout;
SwPipelineLayout SwMaterial::sTransparentPipelineLayout;
std::filesystem::path SwMaterial::GEOMETRY_VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Geometry.vert.spv"};
SwShader SwMaterial::sVertexShader;
std::filesystem::path SwMaterial::GEOMETRY_OPAQUE_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "GeometryOpaque.frag.spv"};
SwShader SwMaterial::sOpaqueFragmentShader;
std::filesystem::path SwMaterial::GEOMETRY_TRANSPARENT_FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "GeometryTransparent.frag.spv"};
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
    if (auto it = sMaterialPipelines.find(materialPipelineOptions); it != sMaterialPipelines.end()) {
        mPipelineBundle = SwGraphicsPipelineBundle(it->second);
        return;
    }

    constructMaterialPipeline(materialPipelineOptions);

    SwPipelinePipeline& retrievedPipeline = sMaterialPipelines.at(materialPipelineOptions);
    mPipelineBundle = SwGraphicsPipelineBundle(retrievedPipeline);

    sLatestMaterialId++;
}

void SwMaterial::init() {
    vk::PushConstantRange materialPushConstantRange =
        SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(SwGeometry::WorkPC));
    std::array<vk::DescriptorSetLayout, 1> materialDescriptorLayouts = {SwMaterialResources::sMaterialResourcesDescriptorLayout.getRawLayout()};
    sOpaquePipelineLayout = SwPipelineFactory::createPipelineLayout(materialDescriptorLayouts, materialPushConstantRange);
    sTransparentPipelineLayout = SwPipelineFactory::createPipelineLayout(materialDescriptorLayouts, materialPushConstantRange);

    sVertexShader = SwShaderFactory::createShader(GEOMETRY_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    sOpaqueFragmentShader = SwShaderFactory::createShader(GEOMETRY_OPAQUE_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);
    sTransparentFragmentShader = SwShaderFactory::createShader(GEOMETRY_TRANSPARENT_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);
}

void SwMaterial::constructMaterialPipeline(SwMaterialPipelineOptions materialPipelineOptions) const {
    vk::CullModeFlags cullMode = materialPipelineOptions.doubleSided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack;
    bool opaque = materialPipelineOptions.alphaMode != fastgltf::AlphaMode::Blend;

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
            //{mRenderer->mScene.mTransparency.mAccumImage.format, accumBlendState}, // TODO implement transparency pass first
            //{mRenderer->mScene.mTransparency.mRevealageImage.format, rvlBlendState}, // TODO implement transparency pass first
        };
    }
    graphicsPipelineOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    graphicsPipelineOptions.mDepthTestEnabled = true;
    graphicsPipelineOptions.mDepthWriteEnabled = opaque;
    graphicsPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;

    auto [it, _] = sMaterialPipelines.try_emplace(materialPipelineOptions, SwGraphicsPipelineFactory::createGraphicsPipeline(graphicsPipelineOptions));
}

void SwMaterial::cleanup() {
    sTransparentFragmentShader.destroy();
    sOpaqueFragmentShader.destroy();
    sVertexShader.destroy();
    sTransparentPipelineLayout.destroy();
    sOpaquePipelineLayout.destroy();
    sMaterialPipelines.clear();
}
