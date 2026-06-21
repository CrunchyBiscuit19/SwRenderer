#include <Renderer/SwRenderer.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwLighting.h>

#include <format>

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::initializeResources() {
    for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
        mResources.mShadowMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("ShadowMap{}", i),
            nullptr,
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            true
        );
    }
    vk::SamplerCreateInfo shadowMapSamplerCreateInfo{};
    shadowMapSamplerCreateInfo.magFilter = vk::Filter::eNearest;
    shadowMapSamplerCreateInfo.minFilter = vk::Filter::eNearest;
    shadowMapSamplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    shadowMapSamplerCreateInfo.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    shadowMapSamplerCreateInfo.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    shadowMapSamplerCreateInfo.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    shadowMapSamplerCreateInfo.minLod = 0.0f;
    shadowMapSamplerCreateInfo.maxLod = vk::LodClampNone;
    shadowMapSamplerCreateInfo.anisotropyEnable = vk::True;
    shadowMapSamplerCreateInfo.maxAnisotropy = SwSamplerFactory::getMaxSamplerAnisotropy();
    shadowMapSamplerCreateInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    shadowMapSamplerCreateInfo.compareEnable = vk::False;
    mResources.mShadowMapsSampler = SwSamplerFactory::createSampler("ShadowMapsSampler", shadowMapSamplerCreateInfo);

    mResources.mShadowMapsDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "ShadowMapsDescriptorLayout",
        {
            {0, vk::DescriptorType::eSampledImage, NUM_LIGHT_CAST_SHADOWS},
            {0, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment 
    );
    mResources.mShadowMapsDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(
        "ShadowMapsDescriptorSet", mResources.mShadowMapsDescriptorLayout, NUM_LIGHT_CAST_SHADOWS
    );

    for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
        mResources.mLightDrawRisIndicesBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("LightDrawRisIndicesBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            SwScene::SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(std::uint32_t),
            true
        );
        mResources.mLightRcsBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            "LightRcsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, SwBatch::RENDER_COMMANDS_INITIAL_BUFFER_SIZE, true
        );
    }

    mResources.mShadowDrawPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowDrawPipelineLayout", nullptr, SwLighting::ShadowDrawPC::getRange());
    SwShader drawVertexShader =
        SwShaderFactory::createShader("ShadowDrawVertexShaderModule", SwLighting::SHADOW_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.blendEnable = VK_FALSE;
    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions drawPipelineOptions;
    drawPipelineOptions.mVertexShader = drawVertexShader.getRawModule();
    drawPipelineOptions.mFragmentShader = nullptr;
    drawPipelineOptions.mLayout = mResources.mShadowDrawPipelineLayout.getRawLayout();
    drawPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    drawPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    drawPipelineOptions.mCullMode = vk::CullModeFlagBits::eFront;
    drawPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    drawPipelineOptions.mMultisamplingEnabled = false;
    drawPipelineOptions.mSampleShadingEnabled = false;
    drawPipelineOptions.mColorAttachments = {};
    drawPipelineOptions.mDepthFormat = SHADOW_MAP_FORMAT;
    drawPipelineOptions.mDepthTestEnabled = true;
    drawPipelineOptions.mDepthWriteEnabled = true;
    drawPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
    mResources.mShadowDrawPipelineBundle =
        SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowDrawPipeline", drawPipelineOptions);

    mResources.mShadowCullPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowCullPipelineLayout", nullptr, SwLighting::ShadowCullPC::getRange());
    SwShader cullShader =
        SwShaderFactory::createShader("ShadowCullShaderModule", SwLighting::SHADOW_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowCullPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ShadowCullPipeline", {cullShader.getRawModule(), mResources.mShadowCullPipelineLayout.getRawLayout()}
    );
}

void SwLighting::System::initializePasses() {
    SwDependency staticDeps;

    for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
        staticDeps.mWriteBuffers.emplace_back(mResources.mLightDrawRisIndicesBuffer[i], SwDependency::BufferDepType::TransferWrite);
        staticDeps.mWriteBuffers.emplace_back(mResources.mLightRcsBuffer[i], SwDependency::BufferDepType::TransferWrite);
    }
    mScene.insertPass(SwPass::Type::LightingShadowReset, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
            cmd.fillBuffer(mResources.mLightDrawRisIndicesBuffer[i].getRawBuffer(), 0, VK_WHOLE_SIZE, 0);
            cmd.fillBuffer(mResources.mLightRcsBuffer[i].getRawBuffer(), 0, VK_WHOLE_SIZE, 0);
        }
    });
    staticDeps.clear();

    mScene.insertPass(SwPass::Type::LightingShadowCull, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
            
    });
    staticDeps.clear();

    mScene.insertPass(SwPass::Type::LightingShadowDraw, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
            
    });
    staticDeps.clear();
}

void SwLighting::System::refreshDynamicDependencies() {}

void SwLighting::System::refreshPushConstants() {
    mResources.mShadowCullPc.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneBoundsBuffer = SwRenderer::sRendererContext.mScene->getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneNodeTransformsBuffer =
        SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();

    mResources.mShadowDrawPc.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneVertexBuffer = SwRenderer::sRendererContext.mScene->getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
}