#include <Renderer/SwSwapchain.h>
#include <Renderer/SwRenderer.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwGeometry.h>
#include <Scene/SwScene.h>

SwGeometry::System::System(SwScene& scene) : SwSystem(scene) {}

void SwGeometry::System::initializeResources() {}

void SwGeometry::System::initializePasses() {
    SwDependency staticDeps;

    // EarlyOpaque 
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(SwPass::Type::GeometryEarlyOpaque, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            SwRenderer::sRendererContext.mSwapchain->getWindowExtent(),
            SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment(),
            SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
        );

        cmd.beginRendering(renderInfo);

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque)) {
            if (batch.getRItems().empty()) {
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
            
            mResources.mWorkPushConstants.mDrawRItemsBuffer = batch.getEarlyRItemsBuffer().getDeviceAddress().value();
            cmd.pushConstants<SwGeometry::WorkPC>(
                batch.getGraphicsPipelineBundle().getRawLayout(), SwGeometry::WorkPC::sStages, 0, mResources.mWorkPushConstants
            );
            
            cmd.drawIndexedIndirectCount(
                batch.getEarlyRItemsBuffer().getRawBuffer(),
                0,
                batch.getEarlyRItemsCount().getRawBuffer(),
                0,
                static_cast<std::uint32_t>(batch.getRItems().size()),
                sizeof(SwRenderItem)
            );
            
            SwRenderer::sRendererContext.mStats->mNumDrawCall++;
        }

        cmd.endRendering();
    });
    staticDeps.clear();

    // LateOpaque
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(SwPass::Type::GeometryLateOpaque, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            SwRenderer::sRendererContext.mSwapchain->getWindowExtent(),
            SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment(),
            SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
        );

        cmd.beginRendering(renderInfo);

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque)) {
            if (batch.getRItems().empty()) {
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

            mResources.mWorkPushConstants.mDrawRItemsBuffer = batch.getFinalRItemsBuffer().getDeviceAddress().value();
            cmd.pushConstants<SwGeometry::WorkPC>(
                batch.getGraphicsPipelineBundle().getRawLayout(), SwGeometry::WorkPC::sStages, 0, mResources.mWorkPushConstants
            );

            cmd.drawIndexedIndirectCount(
                batch.getFinalRItemsBuffer().getRawBuffer(),
                0,
                batch.getFinalRItemsCount().getRawBuffer(),
                0,
                static_cast<std::uint32_t>(batch.getRItems().size()),
                sizeof(SwRenderItem)
            );

            SwRenderer::sRendererContext.mStats->mNumDrawCall++;
            SwRenderer::sRendererContext.mStats->mNumInitialRInsts += batch.getRInsts().size();
        }

        cmd.endRendering();
    });
    staticDeps.clear();

    // Masked
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(SwPass::Type::GeometryMasked, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            SwRenderer::sRendererContext.mSwapchain->getWindowExtent(),
            SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment(),
            SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
        );

        cmd.beginRendering(renderInfo);

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Mask)) {
            if (batch.getRItems().empty()) {
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

            mResources.mWorkPushConstants.mDrawRItemsBuffer = batch.getFinalRItemsBuffer().getDeviceAddress().value();
            cmd.pushConstants<SwGeometry::WorkPC>(
                batch.getGraphicsPipelineBundle().getRawLayout(), SwGeometry::WorkPC::sStages, 0, mResources.mWorkPushConstants
            );

            cmd.drawIndexedIndirectCount(
                batch.getFinalRItemsBuffer().getRawBuffer(),
                0,
                batch.getFinalRItemsCount().getRawBuffer(),
                0,
                static_cast<std::uint32_t>(batch.getRItems().size()),
                sizeof(SwRenderItem)
            );

            SwRenderer::sRendererContext.mStats->mNumDrawCall++;
            SwRenderer::sRendererContext.mStats->mNumInitialRInsts += batch.getRInsts().size();
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
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(SwPass::Type::GeometryTransparent, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            SwRenderer::sRendererContext.mSwapchain->getWindowExtent(),
            {mScene.mWBOIT.getResources().mAccumImage.generateRenderingAttachment(), mScene.mWBOIT.getResources().mRvlImage.generateRenderingAttachment()},
            SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
        );

        cmd.beginRendering(renderInfo);

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Transparent)) {
            if (batch.getRItems().empty()) {
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
            mResources.mWorkPushConstants.mDrawRItemsBuffer = batch.getFinalRItemsBuffer().getDeviceAddress().value();
            cmd.pushConstants<SwGeometry::WorkPC>(
                batch.getGraphicsPipelineBundle().getRawLayout(), SwGeometry::WorkPC::sStages, 0, mResources.mWorkPushConstants
            );
            cmd.drawIndexedIndirectCount(
                batch.getFinalRItemsBuffer().getRawBuffer(),
                0,
                batch.getFinalRItemsCount().getRawBuffer(),
                0,
                static_cast<std::uint32_t>(batch.getRItems().size()),
                sizeof(SwRenderItem)
            );
            SwRenderer::sRendererContext.mStats->mNumDrawCall++;
            SwRenderer::sRendererContext.mStats->mNumInitialRInsts += batch.getRInsts().size();
        }
        cmd.endRendering();
    });
    staticDeps.clear();
}

void SwGeometry::System::refreshDynamicDependencies() {
    SwDependency dynamicDeps;

    // EarlyOpaque
    dynamicDeps.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque)) {
        dynamicDeps.mReadBuffers.emplace_back(&batch.getEarlyRItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getEarlyRItemsCount(), SwDependency::BufferDepType::IndirectRead);
    }
    mScene.mPasses[SwPass::Type::GeometryEarlyOpaque].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // LateOpaque   
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead
    );
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque)) {
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRItemsCount(), SwDependency::BufferDepType::IndirectRead);
    }
    mScene.mPasses[SwPass::Type::GeometryLateOpaque].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // Mask
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead
    );
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Mask)) {
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRItemsCount(), SwDependency::BufferDepType::IndirectRead);
    }
    mScene.mPasses[SwPass::Type::GeometryMasked].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // Transparent
    dynamicDeps.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Transparent)) {
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRItemsCount(), SwDependency::BufferDepType::IndirectRead);
    }
    mScene.mPasses[SwPass::Type::GeometryTransparent].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();
}

void SwGeometry::System::refreshPushConstants() {
    mResources.mWorkPushConstants.mSceneVertexBuffer = mScene.getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneMaterialConstantsBuffer = mScene.getSceneMaterialConstantsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneVisibleRInstsIndicesBuffer =
        mScene.getSceneVisibleRInstsIndicesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
}
