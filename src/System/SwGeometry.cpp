#include <Renderer/SwSwapchain.h>
#include <Renderer/SwRenderer.h>
#include <Resource/SwShader.h>
#include <System/SwGeometry.h>
#include <System/SwLighting.h>
#include <Scene/SwScene.h>

// Issues one indirect draw per non-empty batch. `early` pulls commands from the batch's
// pre-occlusion (early) draw list; otherwise from the final post-occlusion list.
template <typename BatchRange>
void drawBatches(SwScene& scene, SwGeometry::Resources& resources, vk::CommandBuffer cmd, BatchRange&& batches, bool early) {
    for (auto& batch : batches) {
        if (batch.getRcs().empty()) {
            continue;
        }

        auto& itemsBuffer = early ? batch.getEarlyRcsBuffer() : batch.getFinalRcsBuffer();
        auto& countBuffer = early ? batch.getEarlyRcsCount() : batch.getFinalRcsCount();
        auto& pipeline = batch.getGraphicsPipelineBundle();

        cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getRawPipeline());
        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());
        cmd.bindIndexBuffer(scene.getSceneIndexBuffer().getRawBuffer(), 0, vk::IndexType::eUint32);
        cmd.bindDescriptorSets(
            pipeline.getBindPoint(),
            pipeline.getRawLayout(),
            0,
            {scene.getSceneMaterialResourcesDescriptorSet().getRawSet(), scene.getIBLSystem().getConsumeDescriptorSet().getRawSet(),
             scene.getLightingSystem().getSpotShadowMapsDescriptorSet().getRawSet()},
            nullptr
        );

        resources.mWorkPushConstants.mDrawRcsBuffer = itemsBuffer.getDeviceAddress().value();
        cmd.pushConstants<SwGeometry::WorkPC>(pipeline.getRawLayout(), SwGeometry::WorkPC::sStages, 0, resources.mWorkPushConstants);

        cmd.drawIndexedIndirectCount(
            itemsBuffer.getRawBuffer(), 0, countBuffer.getRawBuffer(), 0, static_cast<std::uint32_t>(batch.getRcs().size()), sizeof(SwRenderCommand)
        );

        SwRenderer::sRendererContext.mStats->mNumDrawCall++;
        if (!early) {
            // Counted once per batch; the early opaque pass draws the same opaque batches, so skip it there.
            SwRenderer::sRendererContext.mStats->mNumInitialRis += batch.getRis().size();
        }
    }
}

SwGeometry::System::System(SwScene& scene) : SwSystem(scene) {}

void SwGeometry::System::initializeResources() {}

void SwGeometry::System::initializePasses() {
    // Dependencies shared by every geometry pass: depth (read/write) plus the scene buffers read in the vertex stage.
    auto addCommonDeps = [&](SwDependency& deps) {
        deps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
        deps.mReadImages.emplace_back(&mScene.getIBLSystem().getResources().mIrradianceImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
        deps.mReadImages.emplace_back(&mScene.getIBLSystem().getResources().mPrefilterImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
        for (auto& shadowMap : mScene.getLightingSystem().getResources().mSpotShadowMaps) {
            deps.mReadImages.emplace_back(&shadowMap, SwDependency::ImageDepType::FragmentShaderSampledRead);
        }
        deps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
        deps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        deps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        deps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        deps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        deps.mReadBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        deps.mReadBuffers.emplace_back(&mScene.getSceneLightsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        deps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    };

    SwDependency staticDeps;

    // EarlyOpaque / LateOpaque / Masked all render to the draw image with the standard opaque setup.
    for (auto type : {SwPass::Type::GeometryEarlyOpaque, SwPass::Type::GeometryLateOpaque, SwPass::Type::GeometryMasked}) {
        const bool early = type == SwPass::Type::GeometryEarlyOpaque;
        const SwMaterial::Type materialType = type == SwPass::Type::GeometryMasked ? SwMaterial::Type::Mask : SwMaterial::Type::Opaque;

        staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
        addCommonDeps(staticDeps);
        mScene.insertPass(type, std::move(staticDeps), [&, early, materialType](vk::CommandBuffer cmd) {
            vk::RenderingAttachmentInfo color = SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment();
            vk::RenderingAttachmentInfo depth = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
            cmd.beginRendering(SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), color, depth));
            drawBatches(mScene, mResources, cmd, mScene.getBatchIt(materialType), early);
            cmd.endRendering();
        });
        staticDeps.clear();
    }

    // Transparent renders into the WBOIT accum/reveal targets instead of the draw image.
    staticDeps.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mAccumImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mRvlImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    addCommonDeps(staticDeps);
    mScene.insertPass(SwPass::Type::GeometryTransparent, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 2> colors = {
            mScene.mWBOIT.getResources().mAccumImage.generateRenderingAttachment(),
            mScene.mWBOIT.getResources().mRvlImage.generateRenderingAttachment(),
        };
        vk::RenderingAttachmentInfo depth = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        cmd.beginRendering(SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), colors, depth));
        drawBatches(mScene, mResources, cmd, mScene.getBatchIt(SwMaterial::Type::Transparent), false);
        cmd.endRendering();
    });
    staticDeps.clear();
}

void SwGeometry::System::refreshDynamicDependencies() {
    // Each geometry pass reads the per-frame buffer plus its batches' indirect draw list + count.
    auto setDynamicDeps = [&](SwPass::Type type, auto&& batches, bool early) {
        SwDependency dynamicDeps;
        dynamicDeps.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead
        );
        for (auto& batch : batches) {
            auto& itemsBuffer = early ? batch.getEarlyRcsBuffer() : batch.getFinalRcsBuffer();
            auto& countBuffer = early ? batch.getEarlyRcsCount() : batch.getFinalRcsCount();
            dynamicDeps.mReadBuffers.emplace_back(&itemsBuffer, SwDependency::BufferDepType::IndirectRead);
            dynamicDeps.mReadBuffers.emplace_back(&countBuffer, SwDependency::BufferDepType::IndirectRead);
        }
        mScene.mPasses[type].setDynamicDeps(std::move(dynamicDeps));
    };

    setDynamicDeps(SwPass::Type::GeometryEarlyOpaque, mScene.getBatchIt(SwMaterial::Type::Opaque), true);
    setDynamicDeps(SwPass::Type::GeometryLateOpaque, mScene.getBatchIt(SwMaterial::Type::Opaque), false);
    setDynamicDeps(SwPass::Type::GeometryMasked, mScene.getBatchIt(SwMaterial::Type::Mask), false);
    setDynamicDeps(SwPass::Type::GeometryTransparent, mScene.getBatchIt(SwMaterial::Type::Transparent), false);
}

void SwGeometry::System::refreshPushConstants() {
    mResources.mWorkPushConstants.mSceneVertexBuffer = mScene.getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneMaterialConstantsBuffer = mScene.getSceneMaterialConstantsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneDrawRisIndicesBuffer = mScene.getSceneDrawRisIndicesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneLightsBuffer = mScene.getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mMaxPrefilterMip = mScene.getIBLSystem().getMaxPrefilterMip();
    mResources.mWorkPushConstants.mIblIntensity = mScene.getIBLSystem().getIblIntensity() / mScene.getIBLSystem().getEnvAvgLuminance();
    mResources.mWorkPushConstants.mIblComponents = mScene.getIBLSystem().getIblComponents();
}
