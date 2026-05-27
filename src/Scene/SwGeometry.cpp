#include <Renderer/SwSwapchain.h>
#include <Scene/SwGeometry.h>
#include <Scene/SwScene.h>

#include <ranges>

SwGeometry::System::System(SwScene& scene) : SwSystem(scene) {}

void SwGeometry::System::initializeResources() {
    mResources.mWorkPushConstants.mSceneVertexBuffer = mScene.getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneMaterialConstantsBuffer = mScene.getSceneMaterialConstantsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer().getDeviceAddress().value();
}

void SwGeometry::System::initializePasses() {
    SwDependency deps;

    // Opaque and Masked
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    for (auto& batchType : mScene.getBatchTypes()) {
        if (batchType.first != SwMaterial::Type::Opaque && batchType.first != SwMaterial::Type::Mask) {
            continue;
        }
        for (auto& batch : batchType.second | std::views::values) {
            deps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mScene.insertPass(SwPass::Type::GeometryOpaque, std::move(deps), [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            sRendererContext.mSwapchain->getWindowExtent(),
            sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment(),
            sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
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
                SwPass::setViewportScissors(cmd, vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1});
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
                    SwScene::DRAW_MAX_RENDER_ITEMS,
                    sizeof(SwRenderItem)
                );
                sRendererContext.mStats->mDrawCallCount++;
                sRendererContext.mStats->mPreCullRenderInstancesCount += batch.getRenderInstances().size();
            }
        }

        cmd.endRendering();
    });
    deps.clear();

    // Transparent
    deps.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mAccumImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mRvlImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    for (auto& batchType : mScene.getBatchTypes()) {
        if (batchType.first != SwMaterial::Type::Transparent) {
            continue;
        }
        for (auto& batch : batchType.second | std::views::values) {
            deps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mScene.insertPass(SwPass::Type::GeometryTransparent, std::move(deps), [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            sRendererContext.mSwapchain->getWindowExtent(),
            {mScene.mWBOIT.getResources().mAccumImage.generateRenderingAttachment(), mScene.mWBOIT.getResources().mRvlImage.generateRenderingAttachment()},
            sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
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
                SwPass::setViewportScissors(cmd, vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1});
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
                    SwScene::DRAW_MAX_RENDER_ITEMS,
                    sizeof(SwRenderItem)
                );
                sRendererContext.mStats->mDrawCallCount++;
                sRendererContext.mStats->mPreCullRenderInstancesCount += batch.getRenderInstances().size();
            }
        }
        cmd.endRendering();
    });
    deps.clear();
}