#include <Renderer/SwSwapchain.h>
#include <Renderer/SwRenderer.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwGeometry.h>
#include <Scene/SwScene.h>

#include <ranges>

SwGeometry::System::System(SwScene& scene) : SwSystem(scene) {}

void SwGeometry::System::initializeResources() {
    mResources.mDepthPrePassPipelineLayout =
        SwPipelineFactory::createPipelineLayout(nullptr, SwGeometry::WorkPC::getRange());

    SwShader depthPrePassVertexShader = SwShaderFactory::createShader(DEPTH_PRE_PASS_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);

    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions depthPrePassOptions;
    depthPrePassOptions.mVertexShader = depthPrePassVertexShader.getRawModule();
    depthPrePassOptions.mFragmentShader = std::nullopt;
    depthPrePassOptions.mLayout = mResources.mDepthPrePassPipelineLayout.getRawLayout();
    depthPrePassOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    depthPrePassOptions.mPolygonMode = vk::PolygonMode::eFill;
    depthPrePassOptions.mCullMode = vk::CullModeFlagBits::eBack;
    depthPrePassOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    depthPrePassOptions.mMultisamplingEnabled = false;
    depthPrePassOptions.mSampleShadingEnabled = false;
    depthPrePassOptions.mColorAttachments = {};
    depthPrePassOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    depthPrePassOptions.mDepthTestEnabled = true;
    depthPrePassOptions.mDepthWriteEnabled = true;
    depthPrePassOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
    mResources.mDepthPrePassPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline(depthPrePassOptions);
}

void SwGeometry::System::initializePasses() {
    SwDependency staticDeps;

    // Depth Pre-Pass (opaque only)
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(SwPass::Type::GeometryDepthPrePass, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            SwRenderer::sRendererContext.mSwapchain->getWindowExtent(),
            nullptr,
            SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
        );

        cmd.beginRendering(renderInfo);
        cmd.bindPipeline(mResources.mDepthPrePassPipelineBundle.getBindPoint(), mResources.mDepthPrePassPipelineBundle.getRawPipeline());
        SwPass::setViewportScissors(cmd, vk::Extent3D{SwRenderer::sRendererContext.mSwapchain->getWindowExtent(), 1});
        cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getRawBuffer(), 0, vk::IndexType::eUint32);

        for (auto& batchType : mScene.getBatchTypes()) {
            if (batchType.first != SwMaterial::Type::Opaque) {
                continue;
            }
            for (auto& batch : batchType.second | std::views::values) {
                if (batch.getRItems().empty()) {
                    continue;
                }
                mResources.mWorkPushConstants.mDrawRItemsBuffer = batch.getFrustumRItemsBuffer().getDeviceAddress().value();
                cmd.pushConstants<SwGeometry::WorkPC>(
                    mResources.mDepthPrePassPipelineBundle.getRawLayout(), SwGeometry::WorkPC::sStages, 0, mResources.mWorkPushConstants
                );
                cmd.drawIndexedIndirectCount(
                    batch.getFrustumRItemsBuffer().getRawBuffer(),
                    0,
                    batch.getFrustumRItemsCount().getRawBuffer(),
                    0,
                    static_cast<std::uint32_t>(batch.getRItems().size()),
                    sizeof(SwRenderItem)
                );
                SwRenderer::sRendererContext.mStats->mNumDrawCall++;
                SwRenderer::sRendererContext.mStats->mNumInitialRInsts += batch.getRInsts().size();
            }
        }

        cmd.endRendering();
    });
    staticDeps.clear();

    // Opaque and Masked
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
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

        for (auto& batchType : mScene.getBatchTypes()) {
            if (batchType.first != SwMaterial::Type::Transparent) {
                continue;
            }
            for (auto& batch : batchType.second | std::views::values) {
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
        }
        cmd.endRendering();
    });
    staticDeps.clear();
}

void SwGeometry::System::refreshDynamicDependencies() {
    SwDependency dynamicDeps;

    // DepthPrePass
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead
    );
    for (auto& batchType : mScene.getBatchTypes()) {
        if (batchType.first != SwMaterial::Type::Opaque) continue;
        for (auto& batch : batchType.second | std::views::values) {
            dynamicDeps.mReadBuffers.emplace_back(&batch.getFrustumRItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mScene.mPasses[SwPass::Type::GeometryDepthPrePass].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // GeometryOpaque
    dynamicDeps.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    for (auto& batchType : mScene.getBatchTypes()) {
        if (batchType.first != SwMaterial::Type::Opaque && batchType.first != SwMaterial::Type::Mask) continue;
        for (auto& batch : batchType.second | std::views::values) {
            dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mScene.mPasses[SwPass::Type::GeometryOpaque].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // GeometryTransparent
    dynamicDeps.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    for (auto& batchType : mScene.getBatchTypes()) {
        if (batchType.first != SwMaterial::Type::Transparent) continue;
        for (auto& batch : batchType.second | std::views::values) {
            dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
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
    mResources.mWorkPushConstants.mSceneVisibleRInstsIndicesBuffer =
        mScene.getSceneVisibleRInstsIndicesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
}
