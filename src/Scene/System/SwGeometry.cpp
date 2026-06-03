#include <Renderer/SwSwapchain.h>
#include <Renderer/SwRenderer.h>
#include <Scene/System/SwGeometry.h>
#include <Scene/SwScene.h>

#include <ranges>

SwGeometry::System::System(SwScene& scene) : SwSystem(scene) {}

void SwGeometry::System::initializeResources() {}

void SwGeometry::System::initializePasses() {
    SwDependency staticDeps;

    // Opaque and Masked
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(SwPass::Type::GeometryOpaque, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            SwRenderer::sRendererContext.mSwapchain->getWindowExtent(),
            SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment(),
            SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
        );

        cmd.beginRendering(renderInfo);

        for (auto& batchType : mScene.getBatchTypes()) {
            if (batchType.first != SwMaterial::Type::Opaque && batchType.first != SwMaterial::Type::Mask) {
                continue;
            }
            for (auto& batch : batchType.second | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                cmd.bindPipeline(batch.getGraphicsPipelineBundle().getBindPoint(), batch.getGraphicsPipelineBundle().getRawPipeline());
                SwPass::setViewportScissors(cmd, vk::Extent3D{SwRenderer::sRendererContext.mSwapchain->getWindowExtent(), 1});
                cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getRawBuffer(), 0, vk::IndexType::eUint32);
                cmd.bindDescriptorSets(
                    batch.getGraphicsPipelineBundle().getBindPoint(),
                    batch.getGraphicsPipelineBundle().getRawLayout(),
                    0,
                    mScene.getSceneMaterialResourcesDescriptorSet().getRawSet(),
                    nullptr
                );
                mResources.mWorkPushConstants.mPostCullRenderItemsBuffer = batch.getPostCullRenderItemsBuffer().getDeviceAddress().value();
                cmd.pushConstants<SwGeometry::WorkPC>(
                    batch.getGraphicsPipelineBundle().getRawLayout(), SwGeometry::WorkPC::sStages, 0, mResources.mWorkPushConstants
                );
                cmd.drawIndexedIndirectCount(
                    batch.getPostCullRenderItemsBuffer().getRawBuffer(),
                    0,
                    batch.getPostCullRenderItemsCountBuffer().getRawBuffer(),
                    0,
                    static_cast<std::uint32_t>(batch.getRenderItems().size()),
                    sizeof(SwRenderItem)
                );
                SwRenderer::sRendererContext.mStats->mDrawCallCount++;
                SwRenderer::sRendererContext.mStats->mPreCullRenderInstancesCount += batch.getRenderInstances().size();
            }
        }

        cmd.endRendering();
    });
    staticDeps.clear();

    // Transparent
    staticDeps.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mAccumImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mRvlImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    for (auto& batchType : mScene.getBatchTypes()) {
        if (batchType.first != SwMaterial::Type::Transparent) {
            continue;
        }
        for (auto& batch : batchType.second | std::views::values) {
            staticDeps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mScene.insertPass(SwPass::Type::GeometryTransparent, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            SwRenderer::sRendererContext.mSwapchain->getWindowExtent(),
            {mScene.mWBOIT.getResources().mAccumImage.generateRenderingAttachment(), mScene.mWBOIT.getResources().mRvlImage.generateRenderingAttachment()},
            SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
        );

        cmd.beginRendering(renderInfo);

        for (auto& batchType : mScene.getBatchTypes()) {
            if (batchType.first != SwMaterial::Type::Transparent) {
                continue;
            }
            for (auto& batch : batchType.second | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                cmd.bindPipeline(batch.getGraphicsPipelineBundle().getBindPoint(), batch.getGraphicsPipelineBundle().getRawPipeline());
                SwPass::setViewportScissors(cmd, vk::Extent3D{SwRenderer::sRendererContext.mSwapchain->getWindowExtent(), 1});
                cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getRawBuffer(), 0, vk::IndexType::eUint32);
                cmd.bindDescriptorSets(
                    batch.getGraphicsPipelineBundle().getBindPoint(),
                    batch.getGraphicsPipelineBundle().getRawLayout(),
                    0,
                    mScene.getSceneMaterialResourcesDescriptorSet().getRawSet(),
                    nullptr
                );
                mResources.mWorkPushConstants.mPostCullRenderItemsBuffer = batch.getPostCullRenderItemsBuffer().getDeviceAddress().value();
                cmd.pushConstants<SwGeometry::WorkPC>(
                    batch.getGraphicsPipelineBundle().getRawLayout(), SwGeometry::WorkPC::sStages, 0, mResources.mWorkPushConstants
                );
                cmd.drawIndexedIndirectCount(
                    batch.getPostCullRenderItemsBuffer().getRawBuffer(),
                    0,
                    batch.getPostCullRenderItemsCountBuffer().getRawBuffer(),
                    0,
                    static_cast<std::uint32_t>(batch.getRenderItems().size()),
                    sizeof(SwRenderItem)
                );
                SwRenderer::sRendererContext.mStats->mDrawCallCount++;
                SwRenderer::sRendererContext.mStats->mPreCullRenderInstancesCount += batch.getRenderInstances().size();
            }
        }
        cmd.endRendering();
    });
    staticDeps.clear();
}

void SwGeometry::System::refreshDynamicDependencies() {
    SwDependency dynamicDeps;

    // GeometryOpaque
    dynamicDeps.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    for (auto& batchType : mScene.getBatchTypes()) {
        if (batchType.first != SwMaterial::Type::Opaque && batchType.first != SwMaterial::Type::Mask) continue;
        for (auto& batch : batchType.second | std::views::values) {
            dynamicDeps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mScene.mPasses[SwPass::Type::GeometryOpaque].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // GeometryTransparent
    dynamicDeps.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    for (auto& batchType : mScene.getBatchTypes()) {
        if (batchType.first != SwMaterial::Type::Transparent) continue;
        for (auto& batch : batchType.second | std::views::values) {
            dynamicDeps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mScene.mPasses[SwPass::Type::GeometryTransparent].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();
}

void SwGeometry::System::refreshPushConstants() {
    mResources.mWorkPushConstants.mSceneVertexBuffer = mScene.getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneMaterialConstantsBuffer = mScene.getSceneMaterialConstantsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
}
