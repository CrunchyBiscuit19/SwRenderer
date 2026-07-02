#include <Renderer/SwHelper.h>
#include <Renderer/SwRenderer.h>
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
            {0, vk::DescriptorType::eSampledImage, MAX_ACTIVE_LIGHTS},
            {1, vk::DescriptorType::eSampledImage, MAX_ACTIVE_LIGHTS},
            {2, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
}

void SwLighting::Resources::cleanup() { sShadowConsumeDescriptorLayout.destroy(); }

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::initializeResources() {
    mResources.mActiveLightsBuffer = SwBufferFactory::createAllocatedBuffer(
        "ActiveLightsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, sizeof(ActiveLights), true
    );

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

    for (std::uint32_t i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
        mResources.mShadow2DMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("Shadow2DMap{}", i),
            nullptr,
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            true
        );
    }
    for (std::uint32_t i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
        mResources.mShadowCubeMaps[i] = SwImageFactory::createDepthImageCubemap(
            std::format("ShadowCubeMap{}", i),
            nullptr,
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_CUBE_MAP_WIDTH_HEIGHT, SHADOW_CUBE_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            false
        );
    }

    mResources.mShadowMapsSampler = makeComparisonSampler("ShadowMapsSampler", vk::SamplerAddressMode::eClampToBorder);
    mResources.mShadowMapsDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("ShadowMapsDescriptorSet", Resources::sShadowConsumeDescriptorLayout);
    for (std::uint32_t i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
        mResources.mShadowMapsDescriptorSet.writeImage(
            0, mResources.mShadow2DMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    for (std::uint32_t i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
        mResources.mShadowMapsDescriptorSet.writeImage(
            1, mResources.mShadowCubeMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    mResources.mShadowMapsDescriptorSet.writeSampler(2, mResources.mShadowMapsSampler.getHandle());
    mResources.mShadowMapsDescriptorSet.pushWrites();

    for (std::uint32_t i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
        mResources.mShadowRcsBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("Shadow2DLightRcsBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,  
            SHADOW_INITIAL_RENDER_COMMANDS * sizeof(SwRenderCommand),
            true
        );
        mResources.mShadowRisBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("Shadow2DLightRisBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            SwScene::SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(SwRenderItem),
            true
        );
        mResources.mShadowRisIndicesBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("Shadow2DLightDrawRisIndicesBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            SwScene::SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(std::uint32_t),
            true
        );
    }

    mResources.mActiveLightsSelectionPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ActiveLightsSelectionPipelineLayout", nullptr, SwLighting::ActiveLightsSelectionPC::getRange());
    SwShader activeLightsSelectionShader =
        SwShaderFactory::createShader("ActiveLightsSelectionShaderModule", SwLighting::ACTIVE_LIGHTS_SELECTION_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mActiveLightsSelectionPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ActiveLightsSelectionPipeline", {activeLightsSelectionShader.getHandle(), mResources.mActiveLightsSelectionPipelineLayout.getHandle()}
    );

    mResources.mShadowCullPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowCullPipelineLayout", nullptr, SwLighting::ShadowCullPC::getRange());
    SwShader cullShader = SwShaderFactory::createShader("ShadowCullShaderModule", SwLighting::SHADOW_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowCullPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("ShadowCullPipeline", {cullShader.getHandle(), mResources.mShadowCullPipelineLayout.getHandle()});

    mResources.mShadowDrawPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowDrawPipelineLayout", nullptr, SwLighting::ShadowDrawPC::getRange());
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

    drawPipelineOptions.mVertexEntryPoint = SHADOW_DRAW_OPAQUE_TRANSPARENT_ENTRY_POINT;
    mResources.mShadowDrawOpaqueTransparentPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowDrawPipeline", drawPipelineOptions);

    drawPipelineOptions.mVertexEntryPoint = SHADOW_DRAW_MASKED_ENTRY_POINT;
    mResources.mShadowDrawMaskedPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowDrawPipeline", drawPipelineOptions);
}

void SwLighting::System::initializePasses() {
    SwDependency staticDeps;

    // Shadows Reset
    staticDeps.mWriteBuffers.emplace_back(&mResources.mActiveLightsBuffer, SwDependency::BufferDepType::TransferWrite);
    for (std::uint32_t i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
        staticDeps.mWriteBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::HostWrite);
        staticDeps.mWriteBuffers.emplace_back(&mResources.mShadowRisIndicesBuffer[i], SwDependency::BufferDepType::TransferWrite);
    }
    mScene.insertPass(SwPass::Type::LightingShadowReset, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.fillBuffer(mResources.mActiveLightsBuffer.getHandle(), 0, vk::WholeSize, 0);
        for (std::uint32_t i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
            for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask)) {
                for (auto rc : batch.getRcs()) {
                    rc.mRiCount = 0;
                    mResources.mShadowRcs.emplace_back(rc);
                }
            }
            mResources.mShadowRcsBuffer[i].copyFromUnchecked(mResources.mShadowRcs.data(), mResources.mShadowRcs.size() * sizeof(SwRenderCommand));
            cmd.fillBuffer(mResources.mShadowRisIndicesBuffer[i].getHandle(), 0, vk::WholeSize, 0);
            // TODO fill the mShadowRcs vector whenever generateRcandRis is called
            // TODO create a static staging buffer for mShadowRcsBuffer
            // TODO create a similar reset shader just resetting mRiCount to 0
        }
    });
    staticDeps.clear();

    // Active Lights Selection
    staticDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getCameraBuffer(), SwDependency::BufferDepType::ComputeStorageRead
    );
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mWriteBuffers.emplace_back(&mResources.mActiveLightsBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    mScene.insertPass(SwPass::Type::LightingActiveLightsSelection, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        auto& pipeline = mResources.mActiveLightsSelectionPipelineBundle;
        cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ActiveLightsSelectionPC>(
            pipeline.getLayoutHandle(), SwLighting::ActiveLightsSelectionPC::sStages, 0, mResources.mActiveLightsSelectionPc
        );
        cmd.dispatch(SwHelper::fastDivCeil(MAX_ACTIVE_LIGHTS, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });
    staticDeps.clear();

    // Shadows Cull
    for (std::uint32_t i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
        staticDeps.mReadBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::ComputeStorageReadWrite);
        staticDeps.mWriteBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::ComputeStorageReadWrite);
        staticDeps.mReadBuffers.emplace_back(&mResources.mShadowRisBuffer[i], SwDependency::BufferDepType::ComputeStorageRead);
        staticDeps.mWriteBuffers.emplace_back(&mResources.mShadowRisIndicesBuffer[i], SwDependency::BufferDepType::ComputeStorageWrite);
    }
    staticDeps.mReadBuffers.emplace_back(&mResources.mActiveLightsBuffer, SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    mScene.insertPass(SwPass::Type::LightingShadowCull, std::move(staticDeps), [&](vk::CommandBuffer cmd) {});
    staticDeps.clear();

    // Shadows Draw
    for (std::uint32_t i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
        staticDeps.mWriteImages.emplace_back(&mResources.mShadow2DMaps[i], SwDependency::ImageDepType::DepthAttachmentReadWrite);
        staticDeps.mReadBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::IndirectRead);
        staticDeps.mReadBuffers.emplace_back(&mResources.mShadowRisIndicesBuffer[i], SwDependency::BufferDepType::VertexShaderStorageRead);
    }
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(SwPass::Type::LightingShadowDraw, std::move(staticDeps), [&](vk::CommandBuffer cmd) {});
    staticDeps.clear();
}

void SwLighting::System::refreshDynamicDependencies() {}

void SwLighting::System::refreshPushConstants() {
    mResources.mActiveLightsSelectionPc.mCameraBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getCameraBuffer().getDeviceAddress().value();
    mResources.mActiveLightsSelectionPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mActiveLightsSelectionPc.mSceneNodeTransformsBuffer =
        SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mActiveLightsSelectionPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mActiveLightsSelectionPc.mActiveLightsBuffer = mResources.mActiveLightsBuffer.getDeviceAddress().value();

    mResources.mShadowCullPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneBoundsBuffer = SwRenderer::sRendererContext.mScene->getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mActiveLightsBuffer = mResources.mActiveLightsBuffer.getDeviceAddress().value();

    mResources.mShadowDrawPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneVertexBuffer = SwRenderer::sRendererContext.mScene->getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneMaterialConstantsBuffer = SwRenderer::sRendererContext.mScene->getSceneMaterialConstantsBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mActiveLightsBuffer = mResources.mActiveLightsBuffer.getDeviceAddress().value();
}

/*auto& pipeline = mResources.mShadowCullPipelineBundle;
cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());

const std::vector<AssetLight>& assetLights = mResources.mAssetLights;
for (std::uint32_t slot = 0; slot < mResources.mActiveLightCount; slot++) {
    if (mResources.mShadowType[slot] != ShadowType::TwoD) {
        continue;
    }
    const std::uint32_t mapIndex = mResources.mShadowIndex[slot];
    const SwLight::Params& params = assetLights[mResources.mActiveLightIndices[slot]].mLight->getParams();

    mResources.mShadowCullPc.mLightWorldPos = assetLights[mResources.mActiveLightIndices[slot]].mWorldPosition;
    mResources.mShadowCullPc.mLightRange = params.mRange;
    mResources.mShadowCullPc.mLightType = static_cast<std::uint32_t>(params.mType);
    mResources.mShadowCullPc.mFrustumBuffer =
        mResources.mShadowFrustumBuffer.getDeviceAddress().value() + mapIndex * NUM_FRUSTUM_PLANES * sizeof(SwPlane);
    mResources.mShadowCullPc.mLightDrawRisIndicesBuffer = mResources.mShadowRisIndicesBuffer[mapIndex].getDeviceAddress().value();

    const vk::DeviceAddress rcsBase = mResources.mShadowRcsBuffer[mapIndex].getDeviceAddress().value();
    for (const ShadowBatch& shadowBatch : mResources.mShadowBatches) {
        const std::uint32_t risCount = static_cast<std::uint32_t>(shadowBatch.mBatch->getRis().size());
        if (risCount == 0) {
            continue;
        }
        mResources.mShadowCullPc.mLightRcsBuffer = rcsBase + shadowBatch.mRcsByteOffset;
        mResources.mShadowCullPc.mLightRisBuffer = shadowBatch.mBatch->getRisBuffer().getDeviceAddress().value();
        mResources.mShadowCullPc.mLightRisCount = risCount;
        cmd.pushConstants<SwLighting::ShadowCullPC>(pipeline.getLayoutHandle(), SwLighting::ShadowCullPC::sStages, 0, mResources.mShadowCullPc);
        cmd.dispatch(SwHelper::fastDivCeil(risCount, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    }
}*/

/*for (std::uint32_t slot = 0; slot < mResources.mActiveLightCount; slot++) {
if (mResources.mShadowType[slot] != ShadowType::TwoD) {
continue;
}
const std::uint32_t mapIndex = mResources.mShadowIndex[slot];

vk::RenderingAttachmentInfo depth = mResources.mShadow2DMaps[mapIndex].generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
cmd.beginRendering(SwPass::generateRenderingInfo(vk::Extent2D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT}, {}, depth));
SwPass::setViewportScissors(cmd, vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1});
cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getHandle(), 0, vk::IndexType::eUint32);

mResources.mShadowDrawPc.mLightIndex = slot;
mResources.mShadowDrawPc.mLightDrawRisIndicesBuffer = mResources.mShadowDrawRisIndicesBuffer[mapIndex].getDeviceAddress().value();
const vk::DeviceAddress rcsBase = mResources.mShadowRcsBuffer[mapIndex].getDeviceAddress().value();

SwGraphicsPipelineBundle* bound = nullptr;
for (const ShadowBatch& shadowBatch : mResources.mShadowBatches) {
SwGraphicsPipelineBundle& pipeline =
shadowBatch.mMasked ? mResources.mShadowDrawMaskedPipelineBundle : mResources.mShadowDrawOpaqueTransparentPipelineBundle;
if (bound != &pipeline) {
cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());
bound = &pipeline;
}

mResources.mShadowDrawPc.mLightRcsBuffer = rcsBase + shadowBatch.mRcsByteOffset;
cmd.pushConstants<SwLighting::ShadowDrawPC>(pipeline.getLayoutHandle(), SwLighting::ShadowDrawPC::sStages, 0, mResources.mShadowDrawPc);
cmd.drawIndexedIndirect(
mResources.mShadowRcsBuffer[mapIndex].getHandle(), shadowBatch.mRcsByteOffset, shadowBatch.mRcsCount, sizeof(SwRenderCommand)
);
SwRenderer::sRendererContext.mStats->mNumDrawCall++;
}

cmd.endRendering();
}*/