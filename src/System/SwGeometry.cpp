#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwGeometry.h>
#include <System/SwLighting.h>

std::array<vk::DescriptorSetLayout, 4> SwGeometry::Resources::sGeometrySetLayouts{};

void SwGeometry::Resources::init() {
    sGeometrySetLayouts = {
        SwMaterialResources::sMaterialSamplersDescriptorLayout.getHandle(),
        SwMaterialResources::sMaterialTexturesDescriptorLayout.getHandle(),
        SwIBL::Resources::sConsumeDescriptorLayout.getHandle(),
        SwLighting::Resources::sShadowsConsumeDescriptorLayout.getHandle()
    };
}

void SwGeometry::Resources::cleanup() {}

SwGeometry::System::System(SwScene& scene) : SwSystem(scene) {}

void SwGeometry::System::drawBatches(vk::CommandBuffer cmd, std::array<std::optional<SwMaterial::Type>, SwMaterial::NUM_TYPES> matTypes, bool early) {
    SwAllocatedBuffer& rcsBuffer = early ? mScene.getEarlyRcsBuffer() : mScene.getLateRcsBuffer();
    SwAllocatedBuffer& rcsCount = early ? mScene.getEarlyRcsCount() : mScene.getLateRcsCount();

    for (auto& batch : mScene.getBatchIt(matTypes)) {
        if (batch.getRcsSize() == 0) continue;

        // SV_DrawIndex is relative to each indirect call, so offset the pointer to the batch base to keep shader indexing aligned with the draw offset.
        DrawPC drawPushConstants = mResources.mDrawPushConstants;
        drawPushConstants.mRcsBuffer = rcsBuffer + batch.getRcsIndex() * sizeof(SwRenderCommand);

        auto& pipeline = batch.getGraphicsPipelineBundle();

        cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());
        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());

        cmd.bindIndexBuffer(mScene.getIndexBuffer().getHandle(), 0, vk::IndexType::eUint32);

        cmd.bindDescriptorSets(
            pipeline.getBindPoint(),
            pipeline.getLayoutHandle(),
            0,
            {mScene.getMaterialSamplersDescriptorSet().getHandle(),
             mScene.getMaterialTexturesDescriptorSet().getHandle(),
             mScene.getIBLSystem().getConsumeDescriptorSet().getHandle(),
             mScene.getLightingSystem().getShadowsMapsDescriptorSet().getHandle()},
            nullptr
        );

        cmd.pushConstants<DrawPC>(pipeline.getLayoutHandle(), DrawPC::sStages, 0, drawPushConstants);

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
    zPassOptions.mCullMode = vk::CullModeFlagBits::eBack;  // Not technically correct but close enough for most assets
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
    zPassOptions.mFragmentEntryPoint = std::string(Resources::ZPASS_MASKED_ENTRY_POINT);
    mResources.mZPassMaskedPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("GeometryZPassMaskedPipeline", zPassOptions);
}

void SwGeometry::System::drawZBatches(vk::CommandBuffer cmd, SwGraphicsPipelineBundle& pipeline, SwMaterial::Type matType, bool bindMaterialSets) {
    SwAllocatedBuffer& rcsBuffer = mScene.getLateRcsBuffer();
    SwAllocatedBuffer& rcsCount = mScene.getLateRcsCount();

    for (auto& batch : mScene.getBatchIt({matType})) {
        if (batch.getRcsSize() == 0) continue;

        // SV_DrawIndex is relative to each indirect call, so offset the pointer to the batch base to keep shader indexing aligned with the draw offset.
        DrawPC drawPushConstants = mResources.mDrawPushConstants;
        drawPushConstants.mRcsBuffer = rcsBuffer + batch.getRcsIndex() * sizeof(SwRenderCommand);

        cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());
        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());

        cmd.bindIndexBuffer(mScene.getIndexBuffer().getHandle(), 0, vk::IndexType::eUint32);

        if (bindMaterialSets) {
            cmd.bindDescriptorSets(
                pipeline.getBindPoint(),
                pipeline.getLayoutHandle(),
                0,
                {mScene.getMaterialSamplersDescriptorSet().getHandle(), mScene.getMaterialTexturesDescriptorSet().getHandle()},
                nullptr
            );
        }

        cmd.pushConstants<DrawPC>(pipeline.getLayoutHandle(), DrawPC::sStages, 0, drawPushConstants);

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

void SwGeometry::System::refreshDataUsage() {
    mResources.mDrawPushConstants.mVertexBuffer = mScene.getVertexBuffer();
    mResources.mDrawPushConstants.mMaterialConstantsBuffer = mScene.getMaterialConstantsBuffer();
    mResources.mDrawPushConstants.mNodeTransformsBuffer = mScene.getNodeTransformsBuffer();
    mResources.mDrawPushConstants.mInstancesBuffer = mScene.getInstancesBuffer();
    mResources.mDrawPushConstants.mRisIndicesBuffer = mScene.getRisIndicesBuffer();
    mResources.mDrawPushConstants.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer();
    mResources.mDrawPushConstants.mLightsBuffer = mScene.getLightsBuffer();
    mResources.mDrawPushConstants.mClustersLightIndicesBuffer = mScene.getLightingSystem().getResources().mClustersLightIndicesBuffer;
    mResources.mDrawPushConstants.mClustersLightCounts = mScene.getLightingSystem().getResources().mClustersLightCounts;
    mResources.mDrawPushConstants.mClustersLightOffsetsBuffer = mScene.getLightingSystem().getResources().mClustersLightOffsetsBuffer;
    mResources.mDrawPushConstants.mTargetSize =
        glm::uvec2(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().width, SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().height);
    mResources.mDrawPushConstants.mMaxPrefilterMipLevel = mScene.getIBLSystem().getMaxPrefilterMip();
    mResources.mDrawPushConstants.mIblIntensity = mScene.getIBLSystem().getIblIntensity() / mScene.getIBLSystem().getEnvAvgLuminance();
    mResources.mDrawPushConstants.mIblComponents = static_cast<std::uint32_t>(mScene.getIBLSystem().getIblComponents());

    auto build = [&](SwPass::Type type, bool early, bool transparent, bool depthOnly) {
        SwDependency& d = mScene.mPasses[type].getDeps();
        d.clear();

        if (!depthOnly) {
            if (transparent) {
                d.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mAccumImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
                d.mWriteImages.emplace_back(&mScene.mWBOIT.getResources().mRvlImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
            } else {
                d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
            }
        }
        auto& rcsBuffer = early ? mScene.getEarlyRcsBuffer() : mScene.getLateRcsBuffer();
        auto& rcsCount = early ? mScene.getEarlyRcsCount() : mScene.getLateRcsCount();
        d.mReadBuffers.emplace_back(&rcsBuffer, SwDependency::BufferDepType::IndirectRead);
        d.mReadBuffers.emplace_back(&rcsCount, SwDependency::BufferDepType::IndirectRead);

        d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
        if (!depthOnly) {
            d.mReadImages.emplace_back(&mScene.getIBLSystem().getResources().mIrradianceImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
            d.mReadImages.emplace_back(&mScene.getIBLSystem().getResources().mPrefilterImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
            for (auto& shadowMap : mScene.getLightingSystem().getResources().mDirectionalShadowMaps)
                d.mReadImages.emplace_back(&shadowMap, SwDependency::ImageDepType::FragmentShaderSampledRead);
            for (auto& shadowMap : mScene.getLightingSystem().getResources().mSpotShadowMaps)
                d.mReadImages.emplace_back(&shadowMap, SwDependency::ImageDepType::FragmentShaderSampledRead);
            for (auto& shadowMap : mScene.getLightingSystem().getResources().mPointShadowMaps)
                d.mReadImages.emplace_back(&shadowMap, SwDependency::ImageDepType::FragmentShaderSampledRead);
            d.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
        }
        d.mReadBuffers.emplace_back(&mScene.getVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getRisIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead
        );
        if (!depthOnly) {
            d.mReadBuffers.emplace_back(&mScene.getLightsBuffer(), SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead);
            d.mReadBuffers.emplace_back(
                &mScene.getLightingSystem().getResources().mClustersLightIndicesBuffer, SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead
            );
            d.mReadBuffers.emplace_back(
                &mScene.getLightingSystem().getResources().mClustersLightCounts, SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead
            );
            d.mReadBuffers.emplace_back(
                &mScene.getLightingSystem().getResources().mClustersLightOffsetsBuffer, SwDependency::BufferDepType::VertexAndFragmentShaderStorageRead
            );
        }
        d.mReadBuffers.emplace_back(&mScene.getIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    };

    build(SwPass::Type::GeometryEarlyOpaque, true, false, false);
    build(SwPass::Type::GeometryLateOpaque, false, false, false);
    build(SwPass::Type::GeometryMasked, false, false, false);
    build(SwPass::Type::GeometryTransparent, false, true, false);
    build(SwPass::Type::GeometryZPass, false, false, true);
}
