#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwGeometry.h>
#include <System/SwLighting.h>

std::array<vk::DescriptorSetLayout, 4> SwGeometry::Resources::sGeometrySetLayouts{};

void SwGeometry::Resources::init() {
    SwGeometry::Resources::sGeometrySetLayouts = {
        SwMaterialResources::sMaterialSamplersDescriptorLayout.getHandle(),
        SwMaterialResources::sMaterialTexturesDescriptorLayout.getHandle(),
        SwIBL::Resources::sConsumeDescriptorLayout.getHandle(),
        SwLighting::Resources::sShadowsConsumeDescriptorLayout.getHandle()
    };
}

void SwGeometry::Resources::cleanup() {}

SwGeometry::System::System(SwScene& scene) : SwSystem(scene) {}

void SwGeometry::System::drawBatches(vk::CommandBuffer cmd, std::array<std::optional<SwMaterial::Type>, SwMaterial::NUM_TYPES> matTypes, bool early) {
    SwAllocatedBuffer& rcsBuffer = early ? mScene.getSceneEarlyRcsBuffer() : mScene.getSceneLateRcsBuffer();
    SwAllocatedBuffer& rcsCount = early ? mScene.getSceneEarlyRcsCount() : mScene.getSceneLateRcsCount();

    for (auto& batch : mScene.getBatchIt(matTypes)) {
        if (batch.getRcsSize() == 0) continue;

        // SV_DrawIndex is relative to each indirect call, so offset the pointer to the batch base to keep shader indexing aligned with the draw offset.
        mResources.mDrawPushConstants.mSceneRcsBuffer = rcsBuffer.getDeviceAddress().value() + batch.getRcsIndex() * sizeof(SwRenderCommand);

        auto& pipeline = batch.getGraphicsPipelineBundle();

        cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());
        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());

        cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getHandle(), 0, vk::IndexType::eUint32);

        cmd.bindDescriptorSets(
            pipeline.getBindPoint(),
            pipeline.getLayoutHandle(),
            0,
            {mScene.getSceneMaterialSamplersDescriptorSet().getHandle(),
             mScene.getSceneMaterialTexturesDescriptorSet().getHandle(),
             mScene.getIBLSystem().getConsumeDescriptorSet().getHandle(),
             mScene.getLightingSystem().getShadowsMapsDescriptorSet().getHandle()},
            nullptr
        );

        cmd.pushConstants<SwGeometry::DrawPC>(pipeline.getLayoutHandle(), SwGeometry::DrawPC::sStages, 0, mResources.mDrawPushConstants);

        cmd.drawIndexedIndirectCount(
            rcsBuffer.getHandle(),
            batch.getRcsIndex() * sizeof(SwRenderCommand),
            rcsCount.getHandle(),
            batch.getBatchIndex() * sizeof(std::uint32_t),
            static_cast<std::uint32_t>(batch.getRcsSize()),
            sizeof(SwRenderCommand)
        );

        SwRenderer::sRendererContext.mStats->mNumDrawCall++;
        if (!early) {
            // Counted once per batch; the early opaque pass draws the same opaque batches, so skip it there.
            SwRenderer::sRendererContext.mStats->mNumInitialRis += batch.getRisSize();
        }
    }
}

void SwGeometry::System::initializeResources() {
    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions zPassOptions;
    zPassOptions.mVertexShader = SwMaterial::getGeometryVertexShaderModule();
    zPassOptions.mLayout = SwMaterial::getOpaquePipelineLayoutHandle();
    zPassOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    zPassOptions.mPolygonMode = vk::PolygonMode::eFill;
    zPassOptions.mCullMode = vk::CullModeFlagBits::eBack; // Not technically correct but close enough for most assets
    zPassOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    zPassOptions.mMultisamplingEnabled = false;
    zPassOptions.mSampleShadingEnabled = false;
    zPassOptions.mColorAttachments = {};
    zPassOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    zPassOptions.mDepthTestEnabled = true;
    zPassOptions.mDepthWriteEnabled = true;
    zPassOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;

    zPassOptions.mFragmentShader = std::nullopt;
    mResources.mZPassOpaquePipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("GeometryZPassOpaquePipeline", zPassOptions);

    zPassOptions.mFragmentShader = SwMaterial::getOpaqueMaskedFragmentShaderModule();
    zPassOptions.mFragmentEntryPoint = std::string(SwGeometry::Resources::ZPASS_MASKED_ENTRY_POINT);
    mResources.mZPassMaskedPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("GeometryZPassMaskedPipeline", zPassOptions);
}

void SwGeometry::System::drawZBatches(vk::CommandBuffer cmd, SwGraphicsPipelineBundle& pipeline, SwMaterial::Type matType, bool bindMaterialSets) {
    SwAllocatedBuffer& rcsBuffer = mScene.getSceneLateRcsBuffer();
    SwAllocatedBuffer& rcsCount = mScene.getSceneLateRcsCount();

    for (auto& batch : mScene.getBatchIt({matType})) {
        if (batch.getRcsSize() == 0) continue;

        // SV_DrawIndex is relative to each indirect call, so offset the pointer to the batch base to keep shader indexing aligned with the draw offset.
        mResources.mDrawPushConstants.mSceneRcsBuffer = rcsBuffer.getDeviceAddress().value() + batch.getRcsIndex() * sizeof(SwRenderCommand);

        cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());
        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());

        cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getHandle(), 0, vk::IndexType::eUint32);

        if (bindMaterialSets) {
            cmd.bindDescriptorSets(
                pipeline.getBindPoint(),
                pipeline.getLayoutHandle(),
                0,
                {mScene.getSceneMaterialSamplersDescriptorSet().getHandle(), mScene.getSceneMaterialTexturesDescriptorSet().getHandle()},
                nullptr
            );
        }

        cmd.pushConstants<SwGeometry::DrawPC>(pipeline.getLayoutHandle(), SwGeometry::DrawPC::sStages, 0, mResources.mDrawPushConstants);

        cmd.drawIndexedIndirectCount(
            rcsBuffer.getHandle(),
            batch.getRcsIndex() * sizeof(SwRenderCommand),
            rcsCount.getHandle(),
            batch.getBatchIndex() * sizeof(std::uint32_t),
            static_cast<std::uint32_t>(batch.getRcsSize()),
            sizeof(SwRenderCommand)
        );

        SwRenderer::sRendererContext.mStats->mNumDrawCall++;
    }
}

void SwGeometry::System::initializePasses() {
    // EarlyOpaque / LateOpaque / Masked all render to the draw image with the standard opaque setup.
    for (auto type : {SwPass::Type::GeometryEarlyOpaque, SwPass::Type::GeometryLateOpaque, SwPass::Type::GeometryMasked}) {
        const bool early = type == SwPass::Type::GeometryEarlyOpaque;
        const SwMaterial::Type materialType = type == SwPass::Type::GeometryMasked ? SwMaterial::Type::Mask : SwMaterial::Type::Opaque;

        mScene.insertPass(type, [&, early, materialType](vk::CommandBuffer cmd) {
            vk::RenderingAttachmentInfo color = SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment();
            vk::RenderingAttachmentInfo depth = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
            cmd.beginRendering(SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), color, depth));
            drawBatches(cmd, {materialType}, early);
            cmd.endRendering();
        });
    }

    // Depth pre-pass draw the newly-visible (late-cull) non-transparent geometry into the depth image only.
    mScene.insertPass(SwPass::Type::GeometryZPass, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo depth = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        cmd.beginRendering(SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), {}, depth));
        drawZBatches(cmd, mResources.mZPassOpaquePipelineBundle, SwMaterial::Type::Opaque, false);
        drawZBatches(cmd, mResources.mZPassMaskedPipelineBundle, SwMaterial::Type::Mask, true);
        cmd.endRendering();
    });

    // Transparent renders into the WBOIT accum/reveal targets instead of the draw image.
    mScene.insertPass(SwPass::Type::GeometryTransparent, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 2> colors = {
            mScene.mWBOIT.getResources().mAccumImage.generateRenderingAttachment(),
            mScene.mWBOIT.getResources().mRvlImage.generateRenderingAttachment(),
        };
        vk::RenderingAttachmentInfo depth = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        cmd.beginRendering(SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), colors, depth));
        drawBatches(cmd, {SwMaterial::Type::Transparent}, false);
        cmd.endRendering();
    });
}

void SwGeometry::System::refreshDependencies() {
    // Dependencies shared by every geometry pass: depth (read/write), IBL / shadow maps, the per-frame
    // buffer, and the scene buffers read in the vertex/fragment stages.
    auto addCommonDeps = [&](SwDependency& d) {
        d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
        d.mReadImages.emplace_back(&mScene.getIBLSystem().getResources().mIrradianceImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
        d.mReadImages.emplace_back(&mScene.getIBLSystem().getResources().mPrefilterImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
        for (auto& shadowMap : mScene.getLightingSystem().getResources().mShadows2DMaps) {
            d.mReadImages.emplace_back(&shadowMap, SwDependency::ImageDepType::FragmentShaderSampledRead);
        }
        for (auto& shadowMap : mScene.getLightingSystem().getResources().mShadowsCubeMaps) {
            d.mReadImages.emplace_back(&shadowMap, SwDependency::ImageDepType::FragmentShaderSampledRead);
        }
        d.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
        d.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneRisIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getSceneLightsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneLightsInfoBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    };

    // Each geometry pass adds its color target then its batches' indirect draw list + count.
    auto build = [&](SwPass::Type type, SwMaterial::Type materialType, bool early, bool transparent) {
        SwDependency& d = mScene.mPasses[type].getDeps();
        d.clear();
        if (transparent) {
            d.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mAccumImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
            d.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mRvlImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
        } else {
            d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
        }
        addCommonDeps(d);
        auto& rcsBuffer = early ? mScene.getSceneEarlyRcsBuffer() : mScene.getSceneLateRcsBuffer();
        auto& rcsCount = early ? mScene.getSceneEarlyRcsCount() : mScene.getSceneLateRcsCount();
        d.mReadBuffers.emplace_back(&rcsBuffer, SwDependency::BufferDepType::IndirectRead);
        d.mReadBuffers.emplace_back(&rcsCount, SwDependency::BufferDepType::IndirectRead);
    };

    build(SwPass::Type::GeometryEarlyOpaque, SwMaterial::Type::Opaque, true, false);
    build(SwPass::Type::GeometryLateOpaque, SwMaterial::Type::Opaque, false, false);
    build(SwPass::Type::GeometryMasked, SwMaterial::Type::Mask, false, false);
    build(SwPass::Type::GeometryTransparent, SwMaterial::Type::Transparent, false, true);

    SwDependency& d = mScene.mPasses[SwPass::Type::GeometryZPass].getDeps();
    d.clear();
    d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    d.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    d.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
    d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
    d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
    d.mReadBuffers.emplace_back(&mScene.getSceneRisIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    d.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead
    );
    d.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    d.mReadBuffers.emplace_back(&mScene.getSceneLateRcsBuffer(), SwDependency::BufferDepType::IndirectRead);
    d.mReadBuffers.emplace_back(&mScene.getSceneLateRcsCount(), SwDependency::BufferDepType::IndirectRead);
}

void SwGeometry::System::refreshPushConstants() {
    mResources.mDrawPushConstants.mSceneVertexBuffer = mScene.getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneMaterialConstantsBuffer = mScene.getSceneMaterialConstantsBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneRisIndicesBuffer = mScene.getSceneRisIndicesBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneLightsBuffer = mScene.getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneLightsInfoBuffer = mScene.getSceneLightsInfoBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mMaxPrefilterMipLevel = mScene.getIBLSystem().getMaxPrefilterMip();
    mResources.mDrawPushConstants.mIblIntensity = mScene.getIBLSystem().getIblIntensity() / mScene.getIBLSystem().getEnvAvgLuminance();
    mResources.mDrawPushConstants.mIblComponents = mScene.getIBLSystem().getIblComponents();
}
