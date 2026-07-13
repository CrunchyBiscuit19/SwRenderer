#include <Renderer/SwHelper.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwStagingRing.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwLighting.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

SwDescriptorLayout SwLighting::Resources::sShadowConsumeDescriptorLayout{};

void SwLighting::Resources::init() {
    sShadowConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "ShadowConsumeDescriptorLayout",
        {
            {0, vk::DescriptorType::eSampledImage, MAX_NUM_SHADOW_CASTERS},
            {1, vk::DescriptorType::eSampledImage, MAX_NUM_SHADOW_CASTERS},
            {2, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
}

void SwLighting::Resources::cleanup() { sShadowConsumeDescriptorLayout.destroy(); }

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::initializeResources() {
    mResources.mVisibleLightsBuffer =
        SwBufferFactory::createAllocatedBuffer("VisibleLightsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, INITIAL_ACTIVE_LIGHTS_BUFFER_SIZE, true);

    // Linear filtering gives hardware 2x2 PCF per SampleCmp tap.
    // Opaque-black border so 2D taps outside a frustum read as lit; harmless for seamless cube sampling.
    auto makeComparisonSampler = [](const char* name, vk::SamplerAddressMode addressMode) {
        vk::SamplerCreateInfo info{};
        info.magFilter = vk::Filter::eLinear;
        info.minFilter = vk::Filter::eLinear;
        info.mipmapMode = vk::SamplerMipmapMode::eNearest;
        info.addressModeU = addressMode;
        info.addressModeV = addressMode;
        info.addressModeW = addressMode;
        info.minLod = 0.0f;
        info.maxLod = vk::LodClampNone;
        info.anisotropyEnable = vk::False;
        info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
        info.compareEnable = vk::True;
        info.compareOp = vk::CompareOp::eGreaterOrEqual;
        return SwSamplerFactory::createSampler(name, info);
    };

    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadow2DMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("Shadow2DMap{}", i),
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            true
        );
    }
    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadowCubeMaps[i] = SwImageFactory::createDepthImageCubemap(
            std::format("ShadowCubeMap{}", i),
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_CUBE_MAP_WIDTH_HEIGHT, SHADOW_CUBE_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            false
        );
    }

    mResources.mShadowMapsSampler = makeComparisonSampler("ShadowMapsSampler", vk::SamplerAddressMode::eClampToBorder);
    mResources.mShadowMapsDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("ShadowMapsDescriptorSet", Resources::sShadowConsumeDescriptorLayout);
    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadowMapsDescriptorSet.writeImage(
            0, mResources.mShadow2DMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadowMapsDescriptorSet.writeImage(
            1, mResources.mShadowCubeMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    mResources.mShadowMapsDescriptorSet.writeSampler(2, mResources.mShadowMapsSampler.getHandle());
    mResources.mShadowMapsDescriptorSet.pushWrites();

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            mResources.mShadow2DMaps[i].emitTransition(cmd, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::ImageLayout::eGeneral);
            mResources.mShadowCubeMaps[i].emitTransition(cmd, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::ImageLayout::eGeneral);
        }
    });

    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadowRcsBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("Shadow2DLightRcsBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
            0,
            SHADOW_INITIAL_RENDER_COMMANDS_BUFFER_SIZE,
            true
        );
        mResources.mShadowRisBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("Shadow2DLightRisBuffer{}", i), vk::BufferUsageFlagBits::eStorageBuffer, 0, SwScene::SCENE_INITIAL_RENDER_ITEMS_BUFFER_SIZE, true
        );
        mResources.mShadowRisIndicesBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("Shadow2DLightRisIndicesBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer,
            0,
            SwScene::SCENE_INITIAL_RENDER_ITEMS_INDICES_BUFFER_SIZE,
            true
        );
    }

    mResources.mLightsCullPipelineLayout = SwPipelineFactory::createPipelineLayout("LightsCullPipelineLayout", nullptr, SwLighting::ClusterCullLightsPC::getRange());
    SwShader lightsCullShader = SwShaderFactory::createShader("LightsCullShaderModule", SwLighting::LIGHTS_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mLightsCullPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("LightsCullPipeline", {lightsCullShader.getHandle(), mResources.mLightsCullPipelineLayout.getHandle()});

    mResources.mShadowResetPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ShadowResetPipelineLayout", nullptr, SwLighting::ResetPC::getRange());
    SwShader resetShader = SwShaderFactory::createShader("ShadowResetShaderModule", SwLighting::SHADOW_RESET_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowResetPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("ShadowResetPipeline", {resetShader.getHandle(), mResources.mShadowResetPipelineLayout.getHandle()});

    mResources.mShadowCullPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowCullPipelineLayout", nullptr, SwLighting::CullPC::getRange());
    SwShader cullShader = SwShaderFactory::createShader("ShadowCullShaderModule", SwLighting::SHADOW_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowCullPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("ShadowCullPipeline", {cullShader.getHandle(), mResources.mShadowCullPipelineLayout.getHandle()});

    mResources.mShadowDrawPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowDrawPipelineLayout", nullptr, SwLighting::DrawPC::getRange());
    SwShader drawVertexShader =
        SwShaderFactory::createShader("ShadowDrawVertexShaderModule", SwLighting::SHADOW_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.blendEnable = VK_FALSE;
    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions drawPipelineOptions;
    drawPipelineOptions.mVertexShader = drawVertexShader.getHandle();
    drawPipelineOptions.mFragmentShader = std::nullopt;
    drawPipelineOptions.mLayout = mResources.mShadowDrawPipelineLayout.getHandle();
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

    drawPipelineOptions.mVertexEntryPoint = SHADOW_DRAW_OPAQUE_ENTRY_POINT;
    mResources.mShadowDrawOpaquePipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowDrawPipeline", drawPipelineOptions);

    drawPipelineOptions.mVertexEntryPoint = SHADOW_DRAW_MASKED_ENTRY_POINT;
    mResources.mShadowDrawMaskedPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowDrawPipeline", drawPipelineOptions);
}

void SwLighting::System::initializePasses() {
    // Shadows Reset
    mScene.insertPass(SwPass::Type::LightingShadowReset, [&](vk::CommandBuffer cmd) {
        cmd.fillBuffer(mResources.mVisibleLightsBuffer.getHandle(), 0, vk::WholeSize, 0);
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            cmd.fillBuffer(mResources.mShadowRisIndicesBuffer[i].getHandle(), 0, vk::WholeSize, 0);
        }

        const std::uint32_t rcsCount = static_cast<std::uint32_t>(mResources.mInitialShadowRcs.size());
        if (rcsCount == 0) return;

        auto& resetPipeline = mResources.mShadowResetPipelineBundle;
        cmd.bindPipeline(resetPipeline.getBindPoint(), resetPipeline.getPipelineHandle());
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            mResources.mShadowResetPc.mShadowRcsBuffer = mResources.mShadowRcsBuffer[i].getDeviceAddress().value();
            mResources.mShadowResetPc.mShadowRcsLimit = rcsCount;
            cmd.pushConstants<SwLighting::ResetPC>(resetPipeline.getLayoutHandle(), SwLighting::ResetPC::sStages, 0, mResources.mShadowResetPc);
            cmd.dispatch(SwHelper::fastDivCeil(rcsCount, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
        }
    });

    // Build Clusters

    // Mark Active Clusters

    // Lights Cull
    mScene.insertPass(SwPass::Type::LightingLightsCull, [&](vk::CommandBuffer cmd) {
        auto& pipeline = mResources.mLightsCullPipelineBundle;
        cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ClusterCullLightsPC>(pipeline.getLayoutHandle(), SwLighting::ClusterCullLightsPC::sStages, 0, mResources.mLightsCullPc);
        cmd.dispatch(SwHelper::fastDivCeil(MAX_NUM_SHADOW_CASTERS, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // Shadows Cull
    mScene.insertPass(SwPass::Type::LightingShadowCull, [&](vk::CommandBuffer cmd) {});

    // Shadows Draw
    mScene.insertPass(SwPass::Type::LightingShadowDraw, [&](vk::CommandBuffer cmd) {});
}

void SwLighting::System::regenerateShadowRcs() {
    mResources.mInitialShadowRcs.clear();

    /*for (auto& batch : mScene.getBatchIt({SwMaterial::Type::Opaque, SwMaterial::Type::Mask})) {
        const std::vector<SwRenderCommand>& rcs = batch.getRcs();
        mResources.mInitialShadowRcs.insert(mResources.mInitialShadowRcs.end(), rcs.begin(), rcs.end());
    }*/

    vk::BufferCopy rcsCopy{};
    rcsCopy.srcOffset = 0;
    rcsCopy.dstOffset = 0;
    rcsCopy.size = mResources.mInitialShadowRcs.size() * sizeof(SwRenderCommand);
    if (rcsCopy.size == 0) return;

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this, rcsCopy](vk::CommandBuffer cmd) {
        SwStagingRing* stagingRing = SwRenderer::sRendererContext.mStagingRing;
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            stagingRing->upload(cmd, mResources.mShadowRcsBuffer[i], mResources.mInitialShadowRcs.data(), rcsCopy.size);
        }
    });
}

void SwLighting::System::refreshDependencies() {
    // Shadows Reset
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowReset].getDeps();
        d.clear();
        d.mWriteBuffers.emplace_back(&mResources.mVisibleLightsBuffer, SwDependency::BufferDepType::TransferWrite);
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            d.mWriteBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::ComputeStorageWrite);
            d.mWriteBuffers.emplace_back(&mResources.mShadowRisIndicesBuffer[i], SwDependency::BufferDepType::TransferWrite);
        }
    }

    // Lights Cull
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingLightsCull].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mResources.mVisibleLightsBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Shadows Cull
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowCull].getDeps();
        d.clear();
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            d.mReadBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::ComputeStorageReadWrite);
            d.mWriteBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::ComputeStorageReadWrite);
            d.mReadBuffers.emplace_back(&mResources.mShadowRisBuffer[i], SwDependency::BufferDepType::ComputeStorageRead);
            d.mWriteBuffers.emplace_back(&mResources.mShadowRisIndicesBuffer[i], SwDependency::BufferDepType::ComputeStorageWrite);
        }
        d.mReadBuffers.emplace_back(&mResources.mVisibleLightsBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    }

    // Shadows Draw
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowDraw].getDeps();
        d.clear();
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            d.mWriteImages.emplace_back(&mResources.mShadow2DMaps[i], SwDependency::ImageDepType::DepthAttachmentReadWrite);
            d.mReadBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::IndirectRead);
            d.mReadBuffers.emplace_back(&mResources.mShadowRisIndicesBuffer[i], SwDependency::BufferDepType::VertexShaderStorageRead);
        }
        d.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    }
}

void SwLighting::System::refreshPushConstants() {
    mResources.mLightsCullPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
    mResources.mLightsCullPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mLightsCullPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mLightsCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mLightsCullPc.mVisibleLightsBuffer = mResources.mVisibleLightsBuffer.getDeviceAddress().value();

    mResources.mShadowCullPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneBoundsBuffer = SwRenderer::sRendererContext.mScene->getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mVisibleLightsBuffer = mResources.mVisibleLightsBuffer.getDeviceAddress().value();

    mResources.mShadowDrawPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneVertexBuffer = SwRenderer::sRendererContext.mScene->getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneMaterialConstantsBuffer = SwRenderer::sRendererContext.mScene->getSceneMaterialConstantsBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mVisibleLightsBuffer = mResources.mVisibleLightsBuffer.getDeviceAddress().value();
}