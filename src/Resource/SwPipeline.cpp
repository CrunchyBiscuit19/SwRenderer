#include <Renderer/SwRenderer.h>
#include <Resource/SwPipeline.h>

SwPipelineLayout::SwPipelineLayout(): mLayout(nullptr) {}

SwPipelineLayout::SwPipelineLayout(vk::raii::PipelineLayout layout) : mLayout(std::move(layout)) {}

void SwPipelineLayout::destroy() { mLayout.clear(); }

std::uint32_t SwPipelineBundle::sLatestPipelineID{0};

SwPipelineBundle::SwPipelineBundle(): mPipeline(nullptr) {}

SwPipelineBundle::SwPipelineBundle(vk::raii::Pipeline pipeline, vk::PipelineLayout layout)
    : mId{sLatestPipelineID++}, mPipeline(std::move(pipeline)), mLayout(layout) {}

SwGraphicsPipelineBundle::SwGraphicsPipelineBundle(vk::raii::Pipeline pipeline, vk::PipelineLayout layout) : SwPipelineBundle(std::move(pipeline), layout) {}

SwComputePipelineBundle::SwComputePipelineBundle(vk::raii::Pipeline pipeline, vk::PipelineLayout layout) : SwPipelineBundle(std::move(pipeline), layout) {}

SwRendererContext SwPipelineFactory::sRendererContext{};

void SwPipelineFactory::init(SwRendererContext context) {
    sRendererContext = context;
}

SwPipelineLayout SwPipelineFactory::createPipelineLayout(
    vk::ArrayProxy<vk::DescriptorSetLayout> layouts, vk::ArrayProxy<vk::PushConstantRange> pushConstantRanges
) {
    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo({}, layouts.size(), layouts.data(), pushConstantRanges.size(), pushConstantRanges.data());
    return SwPipelineLayout(vk::raii::PipelineLayout(*sRendererContext.mDevice, pipelineLayoutCreateInfo));
}

SwGraphicsPipelineBundle SwGraphicsPipelineFactory::createGraphicsPipeline(SwGraphicsPipelineOptions options) {
    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.pNext = nullptr;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineDynamicStateCreateInfo dynamicInfo;
    std::array<vk::DynamicState, 2> state = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    dynamicInfo.pDynamicStates = state.data();
    dynamicInfo.dynamicStateCount = state.size();

    vk::PipelineVertexInputStateCreateInfo pipelineVertexStateCreateInfo;

    std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos;
    std::uint32_t numStages = MIN_NUM_SHADER_STAGES;
    vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo;
    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo;
    vk::PipelineMultisampleStateCreateInfo pipelineMultiSampleStateCreateInfo;
    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
    vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo;
    vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo;

    setShaders(pipelineShaderStageCreateInfos, numStages, options.mVertexShader, options.mFragmentShader);
    setInputTopology(pipelineInputAssemblyStateCreateInfo, options.mTopology);
    setPolygonMode(pipelineRasterizationStateCreateInfo, options.mPolygonMode);
    setCullMode(pipelineRasterizationStateCreateInfo, options.mCullMode, options.mFrontFace);
    options.mMultisamplingEnabled ? enableMultisampling(pipelineMultiSampleStateCreateInfo) : disableMultisampling(pipelineMultiSampleStateCreateInfo);
    options.mSampleShadingEnabled ? enableSampleShading(pipelineMultiSampleStateCreateInfo) : disableSampleShading(pipelineMultiSampleStateCreateInfo);
    std::vector<vk::Format> formats;
    std::vector<vk::PipelineColorBlendAttachmentState> blendStates;
    formats.reserve(options.mColorAttachments.size());
    blendStates.reserve(options.mColorAttachments.size());
    for (auto& [format, blendState] : options.mColorAttachments) {
        formats.emplace_back(format);
        blendStates.emplace_back(blendState);
    }
    setColorAttachments(pipelineRenderingCreateInfo, pipelineColorBlendStateCreateInfo, formats, blendStates);
    setDepthFormat(pipelineRenderingCreateInfo, options.mDepthFormat);
    options.mDepthTestEnabled ? enableDepthTest(pipelineDepthStencilStateCreateInfo, options.mDepthWriteEnabled, options.mDepthCompareOp)
                              : disableDepthTest(pipelineDepthStencilStateCreateInfo);

    vk::GraphicsPipelineCreateInfo graphicsPipelineInfo = {};
    graphicsPipelineInfo.pNext = &pipelineRenderingCreateInfo;
    graphicsPipelineInfo.stageCount = numStages;
    graphicsPipelineInfo.pStages = pipelineShaderStageCreateInfos.data();
    graphicsPipelineInfo.pVertexInputState = &pipelineVertexStateCreateInfo;
    graphicsPipelineInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
    graphicsPipelineInfo.pViewportState = &viewportState;
    graphicsPipelineInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
    graphicsPipelineInfo.pMultisampleState = &pipelineMultiSampleStateCreateInfo;
    graphicsPipelineInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
    graphicsPipelineInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
    graphicsPipelineInfo.layout = options.mLayout;
    graphicsPipelineInfo.pDynamicState = &dynamicInfo;

    return SwGraphicsPipelineBundle(vk::raii::Pipeline(sRendererContext.mDevice->createGraphicsPipeline(nullptr, graphicsPipelineInfo)), options.mLayout);
}

void SwGraphicsPipelineFactory::setShaders(
    std::vector<vk::PipelineShaderStageCreateInfo>& pipelineShaderStageCreateInfos, std::uint32_t& numStages, vk::ShaderModule vertexShader,
    vk::ShaderModule fragmentShader
) {
    pipelineShaderStageCreateInfos.clear();
    pipelineShaderStageCreateInfos.emplace_back(
        vk::PipelineShaderStageCreateFlags{}, vk::ShaderStageFlagBits::eVertex, vertexShader, DEFAULT_SHADER_ENTRY_POINT.c_str()
    );
    pipelineShaderStageCreateInfos.emplace_back(
        vk::PipelineShaderStageCreateFlags{}, vk::ShaderStageFlagBits::eFragment, fragmentShader, DEFAULT_SHADER_ENTRY_POINT.c_str()
    );
}

void SwGraphicsPipelineFactory::setInputTopology(
    vk::PipelineInputAssemblyStateCreateInfo& pipelineInputAssemblyStateCreateInfo, vk::PrimitiveTopology topology
) {
    pipelineInputAssemblyStateCreateInfo.topology = topology;
    pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;
}

void SwGraphicsPipelineFactory::setPolygonMode(vk::PipelineRasterizationStateCreateInfo& pipelineRasterizationStateCreateInfo, vk::PolygonMode mode) {
    pipelineRasterizationStateCreateInfo.polygonMode = mode;
    pipelineRasterizationStateCreateInfo.lineWidth = 1.f;
}

void SwGraphicsPipelineFactory::setCullMode(
    vk::PipelineRasterizationStateCreateInfo& pipelineRasterizationStateCreateInfo, vk::CullModeFlags cullMode, vk::FrontFace frontFace
) {
    pipelineRasterizationStateCreateInfo.cullMode = cullMode;
    pipelineRasterizationStateCreateInfo.frontFace = frontFace;
}

void SwGraphicsPipelineFactory::disableMultisampling(vk::PipelineMultisampleStateCreateInfo& pipelineMultiSampleStateCreateInfo) {
    pipelineMultiSampleStateCreateInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
    pipelineMultiSampleStateCreateInfo.pSampleMask = nullptr;
    pipelineMultiSampleStateCreateInfo.alphaToCoverageEnable = vk::False;
    pipelineMultiSampleStateCreateInfo.alphaToOneEnable = vk::False;
}

void SwGraphicsPipelineFactory::enableMultisampling(vk::PipelineMultisampleStateCreateInfo& pipelineMultiSampleStateCreateInfo) {
    pipelineMultiSampleStateCreateInfo.rasterizationSamples = vk::SampleCountFlagBits::e4;
}

void SwGraphicsPipelineFactory::disableSampleShading(vk::PipelineMultisampleStateCreateInfo& pipelineMultiSampleStateCreateInfo) {
    pipelineMultiSampleStateCreateInfo.sampleShadingEnable = vk::False;
    pipelineMultiSampleStateCreateInfo.minSampleShading = 1.0f;
}

void SwGraphicsPipelineFactory::enableSampleShading(vk::PipelineMultisampleStateCreateInfo& pipelineMultiSampleStateCreateInfo) {
    pipelineMultiSampleStateCreateInfo.sampleShadingEnable = vk::True;
    pipelineMultiSampleStateCreateInfo.minSampleShading = 1.0f;
}

void SwGraphicsPipelineFactory::setColorAttachments(
    vk::PipelineRenderingCreateInfo& pipelineRenderingCreateInfo, vk::PipelineColorBlendStateCreateInfo& pipelineColorBlendStateCreateInfo,
    std::span<vk::Format> colorAttachmentFormats, std::span<vk::PipelineColorBlendAttachmentState> colorBlendAttachmentStates
) {
    pipelineColorBlendStateCreateInfo.pNext = nullptr;
    pipelineColorBlendStateCreateInfo.logicOpEnable = vk::False;
    pipelineColorBlendStateCreateInfo.logicOp = vk::LogicOp::eCopy;
    pipelineColorBlendStateCreateInfo.setAttachments(colorBlendAttachmentStates);

    pipelineRenderingCreateInfo.setColorAttachmentFormats(colorAttachmentFormats);
}

void SwGraphicsPipelineFactory::setDepthFormat(vk::PipelineRenderingCreateInfo& pipelineRenderingCreateInfo, vk::Format format) {
    pipelineRenderingCreateInfo.depthAttachmentFormat = format;
}

void SwGraphicsPipelineFactory::disableDepthTest(vk::PipelineDepthStencilStateCreateInfo& pipelineDepthStencilStateCreateInfo) {
    pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.depthCompareOp = vk::CompareOp::eNever;
    pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.front = vk::StencilOpState{};
    pipelineDepthStencilStateCreateInfo.back = vk::StencilOpState{};
    pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.f;
    pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.f;
}

void SwGraphicsPipelineFactory::enableDepthTest(
    vk::PipelineDepthStencilStateCreateInfo& pipelineDepthStencilStateCreateInfo, bool depthWriteEnable, vk::CompareOp op
) {
    pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
    pipelineDepthStencilStateCreateInfo.depthCompareOp = op;
    pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.front = vk::StencilOpState{};
    pipelineDepthStencilStateCreateInfo.back = vk::StencilOpState{};
    pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.f;
    pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.f;
}

SwComputePipelineBundle SwComputePipelineFactory::createComputePipeline(SwComputePipelineOptions options) {
    vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo;

    setShaders(pipelineShaderStageCreateInfo, options.mComputeShader);

    vk::ComputePipelineCreateInfo computePipelineInfo{};
    computePipelineInfo.layout = options.mLayout;
    computePipelineInfo.stage = pipelineShaderStageCreateInfo;
    computePipelineInfo.pNext = nullptr;

    return SwComputePipelineBundle(vk::raii::Pipeline(sRendererContext.mDevice->createComputePipeline(nullptr, computePipelineInfo)), options.mLayout);
}

void SwComputePipelineFactory::setShaders(vk::PipelineShaderStageCreateInfo& pipelineShaderStageCreateInfo, vk::ShaderModule computeShader) {
    pipelineShaderStageCreateInfo.stage = vk::ShaderStageFlagBits::eCompute;
    pipelineShaderStageCreateInfo.module = computeShader;
    pipelineShaderStageCreateInfo.pName = DEFAULT_SHADER_ENTRY_POINT.data();
}