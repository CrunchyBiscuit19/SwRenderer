#include <Renderer/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <System/SwCull.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include <format>

SwCull::System::System(SwScene& scene) : SwSystem(scene) {}

void SwCull::System::initializeOtherPasses() {
    // PrepOcclusion
    mScene.insertPass(SwPass::Type::CullPrepOcclusion, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mPrepOcclusionPipelineBundle.getBindPoint(), mResources.mPrepOcclusionPipelineBundle.getPipelineHandle());

        cmd.bindDescriptorSets(
            mResources.mPrepOcclusionPipelineBundle.getBindPoint(),
            mResources.mPrepOcclusionPipelineBundle.getLayoutHandle(),
            0,
            mResources.mPrepOcclusionDescriptorSet.getHandle(),
            nullptr
        );

        mResources.mPrepOcclusionPushConstants.mLevel = -1;
        cmd.pushConstants<SwCull::PrepOcclusionPC>(
            mResources.mPrepOcclusionPipelineBundle.getLayoutHandle(), SwCull::PrepOcclusionPC::sStages, 0, mResources.mPrepOcclusionPushConstants
        );

        cmd.dispatch(
            SwHelper::fastDivCeil(mResources.mDepthPyramidImage.getExtent().width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(mResources.mDepthPyramidImage.getExtent().height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );

        mResources.mDepthPyramidImage.emitTransition(
            cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead, vk::ImageLayout::eGeneral, 0
        );

        for (std::uint32_t i = 0; i < mResources.mDepthPyramidLevels - 1; i++) {
            mResources.mPrepOcclusionPushConstants.mLevel = i;
            cmd.pushConstants<SwCull::PrepOcclusionPC>(
                mResources.mPrepOcclusionPipelineBundle.getLayoutHandle(), SwCull::PrepOcclusionPC::sStages, 0, mResources.mPrepOcclusionPushConstants
            );

            const std::uint32_t dstW = std::max(1u, mResources.mDepthPyramidImage.getExtent().width >> (i + 1));
            const std::uint32_t dstH = std::max(1u, mResources.mDepthPyramidImage.getExtent().height >> (i + 1));
            cmd.dispatch(
                SwHelper::fastDivCeil(dstW, SwRenderer::MAX_2D_WORKGROUP_THREADS), SwHelper::fastDivCeil(dstH, SwRenderer::MAX_2D_WORKGROUP_THREADS), 1
            );

            if (i < mResources.mDepthPyramidLevels - 1 - 1) {
                mResources.mDepthPyramidImage.emitTransition(
                    cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead, vk::ImageLayout::eGeneral, i + 1
                );
            }
        }
    });

    // PublishCount
    mScene.insertPass(SwPass::Type::CullPublishCount, [&](vk::CommandBuffer cmd) {
        const vk::BufferCopy region{0, 0, sizeof(std::uint32_t)};
        cmd.copyBuffer(
            SwRenderer::sRendererContext.mStats->mRisScratchCount.getHandle(), SwRenderer::sRendererContext.mStats->mRisPublishedCount.getHandle(), region
        );
    });
}

void SwCull::System::initializeEarlyPasses() {
    // EarlyReset
    mScene.insertPass(SwPass::Type::CullEarlyReset, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mResetPipelineBundle.getBindPoint(), mResources.mResetPipelineBundle.getPipelineHandle());

        cmd.fillBuffer(SwRenderer::sRendererContext.mStats->mRisScratchCount.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getRisIndicesBuffer().getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getVisibilityRisWriteBuffer().getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getEarlyRcsBuffer().getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getEarlyRcsCount().getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getLateRcsBuffer().getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getLateRcsCount().getHandle(), 0, vk::WholeSize, 0);

        cmd.pushConstants<SwCull::ResetPC>(mResources.mResetPipelineBundle.getLayoutHandle(), SwCull::ResetPC::sStages, 0, mResources.mResetPushConstants);

        std::uint32_t groupCountX = SwHelper::fastDivCeil(mResources.mResetPushConstants.mRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS);
        if (groupCountX == 0) return;
        cmd.dispatch(groupCountX, 1, 1);
    });

    // EarlyTest
    mScene.insertPass(SwPass::Type::CullEarlyTest, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mTestPipelineBundle.getBindPoint(), mResources.mTestPipelineBundle.getPipelineHandle());

        cmd.bindDescriptorSets(
            mResources.mTestPipelineBundle.getBindPoint(),
            mResources.mTestPipelineBundle.getLayoutHandle(),
            0,
            mResources.mTestDescriptorSet.getHandle(),
            nullptr
        );

        mResources.mTestPushConstants.mPhase = SwCull::Phase::Early;
        cmd.pushConstants<SwCull::TestPC>(mResources.mTestPipelineBundle.getLayoutHandle(), SwCull::TestPC::sStages, 0, mResources.mTestPushConstants);

        std::uint32_t groupCountX = SwHelper::fastDivCeil(mResources.mTestPushConstants.mRisLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS);
        if (groupCountX == 0) return;
        cmd.dispatch(groupCountX, 1, 1);
    });

    // EarlyCompact
    mScene.insertPass(SwPass::Type::CullEarlyCompact, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getPipelineHandle());

        mResources.mCompactPushConstants.mPostRcsBuffer = mScene.getEarlyRcsBuffer().getDeviceAddress().value();
        mResources.mCompactPushConstants.mPostRcsCount = mScene.getEarlyRcsCount().getDeviceAddress().value();
        cmd.pushConstants<SwCull::CompactPC>(
            mResources.mCompactPipelineBundle.getLayoutHandle(), SwCull::CompactPC::sStages, 0, mResources.mCompactPushConstants
        );

        std::uint32_t groupCountX = SwHelper::fastDivCeil(mResources.mCompactPushConstants.mPreRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS);
        if (groupCountX == 0) return;
        cmd.dispatch(groupCountX, 1, 1);
    });
}

void SwCull::System::initializeLatePasses() {
    // LateReset
    // Zero initialRcs.mRiCount after the early compact has snapshotted it. Late draw list limited to just the newly-visible delta.
    mScene.insertPass(SwPass::Type::CullLateReset, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mResetPipelineBundle.getBindPoint(), mResources.mResetPipelineBundle.getPipelineHandle());

        cmd.pushConstants<SwCull::ResetPC>(mResources.mResetPipelineBundle.getLayoutHandle(), SwCull::ResetPC::sStages, 0, mResources.mResetPushConstants);

        std::uint32_t groupCountX = SwHelper::fastDivCeil(mResources.mResetPushConstants.mRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS);
        if (groupCountX == 0) return;
        cmd.dispatch(groupCountX, 1, 1);
    });

    // LateTest
    mScene.insertPass(SwPass::Type::CullLateTest, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mTestPipelineBundle.getBindPoint(), mResources.mTestPipelineBundle.getPipelineHandle());

        cmd.bindDescriptorSets(
            mResources.mTestPipelineBundle.getBindPoint(),
            mResources.mTestPipelineBundle.getLayoutHandle(),
            0,
            mResources.mTestDescriptorSet.getHandle(),
            nullptr
        );

        mResources.mTestPushConstants.mPhase = SwCull::Phase::Late;
        cmd.pushConstants<SwCull::TestPC>(mResources.mTestPipelineBundle.getLayoutHandle(), SwCull::TestPC::sStages, 0, mResources.mTestPushConstants);

        std::uint32_t groupCountX = SwHelper::fastDivCeil(mResources.mTestPushConstants.mRisLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS);
        if (groupCountX == 0) return;
        cmd.dispatch(groupCountX, 1, 1);
    });

    // LateCompact
    mScene.insertPass(SwPass::Type::CullLateCompact, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getPipelineHandle());

        mResources.mCompactPushConstants.mPostRcsBuffer = mScene.getLateRcsBuffer().getDeviceAddress().value();
        mResources.mCompactPushConstants.mPostRcsCount = mScene.getLateRcsCount().getDeviceAddress().value();
        cmd.pushConstants<SwCull::CompactPC>(
            mResources.mCompactPipelineBundle.getLayoutHandle(), SwCull::CompactPC::sStages, 0, mResources.mCompactPushConstants
        );

        std::uint32_t groupCountX = SwHelper::fastDivCeil(mResources.mCompactPushConstants.mPreRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS);
        if (groupCountX == 0) return;
        cmd.dispatch(groupCountX, 1, 1);
    });
}

void SwCull::System::initializeResources() {
    // Reset*
    mResources.mResetPipelineLayout = SwPipelineFactory::createPipelineLayout("CullResetPipelineLayout", nullptr, SwCull::ResetPC::getRange());
    SwShader resetShader = SwShaderFactory::createShader("CullResetShaderModule", RESET_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mResetPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("CullResetPipeline", {resetShader.getHandle(), mResources.mResetPipelineLayout.getHandle()});

    // Compact*
    mResources.mCompactPipelineLayout = SwPipelineFactory::createPipelineLayout("CullCompactPipelineLayout", nullptr, SwCull::CompactPC::getRange());
    SwShader compactShader = SwShaderFactory::createShader("CullCompactShaderModule", COMPACT_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mCompactPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("CullCompactPipeline", {compactShader.getHandle(), mResources.mCompactPipelineLayout.getHandle()});

    // PrepOcclusion
    mResources.mPrepOcclusionDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "CullPrepOcclusionDescriptorSetLayout",
        {{0, vk::DescriptorType::eSampledImage, 1},
         {1, vk::DescriptorType::eSampledImage, MAX_DEPTH_PYRAMID_LEVELS},
         {2, vk::DescriptorType::eStorageImage, MAX_DEPTH_PYRAMID_LEVELS},
         {3, vk::DescriptorType::eSampler, 1}},
        vk::ShaderStageFlagBits::eCompute
    );

    vk::SamplerReductionModeCreateInfo reductionInfo{vk::SamplerReductionMode::eMin};
    vk::SamplerCreateInfo minSamplerInfo{};
    minSamplerInfo.setPNext(&reductionInfo);
    minSamplerInfo.setMagFilter(vk::Filter::eLinear);
    minSamplerInfo.setMinFilter(vk::Filter::eLinear);
    minSamplerInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
    minSamplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    minSamplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    minSamplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    mResources.mDepthPyramidMinSampler = SwSamplerFactory::createSampler("CullDepthPyramidMinSampler", minSamplerInfo);

    mResources.mPrepOcclusionDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("CullPrepOcclusionDescriptorSet", mResources.mPrepOcclusionDescriptorLayout);
    mResources.mPrepOcclusionDescriptorSet.writeSampler(3, mResources.mDepthPyramidMinSampler.getHandle());

    mResources.mPrepOcclusionPipelineLayout = SwPipelineFactory::createPipelineLayout(
        "CullPrepOcclusionPipelineLayout", mResources.mPrepOcclusionDescriptorLayout.getHandle(), SwCull::PrepOcclusionPC::getRange()
    );
    SwShader depthPyramidShader =
        SwShaderFactory::createShader("CullPrepOcclusionShaderModule", PREP_OCCLUSION_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mPrepOcclusionPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "CullPrepOcclusionPipeline", {depthPyramidShader.getHandle(), mResources.mPrepOcclusionPipelineLayout.getHandle()}
    );

    // Test*
    mResources.mTestDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "CullTestDescriptorSetLayout", {{0, vk::DescriptorType::eSampledImage, 1}, {1, vk::DescriptorType::eSampler, 1}}, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mTestDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("CullTestDescriptorSet", mResources.mTestDescriptorLayout);

    vk::SamplerReductionModeCreateInfo testReductionInfo{vk::SamplerReductionMode::eMin};
    vk::SamplerCreateInfo testSamplerInfo{};
    testSamplerInfo.setPNext(&testReductionInfo);
    testSamplerInfo.setMagFilter(vk::Filter::eLinear);
    testSamplerInfo.setMinFilter(vk::Filter::eLinear);
    testSamplerInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
    testSamplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    testSamplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    testSamplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    testSamplerInfo.setMaxLod(VK_LOD_CLAMP_NONE);
    mResources.mTestDepthPyramidSampler = SwSamplerFactory::createSampler("CullTestDepthPyramidSampler", testSamplerInfo);
    mResources.mTestDescriptorSet.writeSampler(1, mResources.mTestDepthPyramidSampler.getHandle());

    mResources.mTestPipelineLayout =
        SwPipelineFactory::createPipelineLayout("CullTestPipelineLayout", mResources.mTestDescriptorLayout.getHandle(), SwCull::TestPC::getRange());
    SwShader testShader = SwShaderFactory::createShader("CullTestShaderModule", TEST_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mTestPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("CullTestPipeline", {testShader.getHandle(), mResources.mTestPipelineLayout.getHandle()});

    reInitializeOnResize();
}

void SwCull::System::initializePasses() {
    initializeOtherPasses();
    initializeEarlyPasses();
    initializeLatePasses();
}

void SwCull::System::refreshDependencies() {
    // PrepOcclusion
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullPrepOcclusion].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::ComputeShaderSampledRead);
        d.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);
        d.mWriteImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);
    }

    // PublishCount
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullPublishCount].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::TransferRead);
        d.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisPublishedCount, SwDependency::BufferDepType::TransferWrite);
    }

    // EarlyReset
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullEarlyReset].getDeps();
        d.clear();
        d.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getRisIndicesBuffer(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getEarlyRcsBuffer(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getEarlyRcsCount(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getLateRcsBuffer(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getLateRcsCount(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getVisibilityRisWriteBuffer(), SwDependency::BufferDepType::TransferWrite);
    }

    // EarlyTest
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullEarlyTest].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeShaderSampledRead);
        d.mReadBuffers.emplace_back(&mScene.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mScene.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mReadBuffers.emplace_back(&mScene.getRisBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getBatchesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getVisibilityRisReadBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getRisIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getVisibilityRisWriteBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // EarlyCompact
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullEarlyCompact].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mScene.getBatchesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mScene.getEarlyRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getEarlyRcsCount(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // LateReset
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullLateReset].getDeps();
        d.clear();
        d.mWriteBuffers.emplace_back(&mScene.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // LateTest
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullLateTest].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeShaderSampledRead);
        d.mReadBuffers.emplace_back(&mScene.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mScene.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mReadBuffers.emplace_back(&mScene.getRisBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getBatchesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getVisibilityRisReadBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getRisIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getVisibilityRisWriteBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // LateCompact
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullLateCompact].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mScene.getBatchesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mScene.getLateRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getLateRcsCount(), SwDependency::BufferDepType::ComputeStorageWrite);
    }
}

void SwCull::System::refreshPushConstants() {
    mResources.mResetPushConstants.mRcsBuffer = mScene.getInitialRcsBuffer().getDeviceAddress().value();
    mResources.mResetPushConstants.mRcsLimit = static_cast<std::uint32_t>(mScene.getRcs().size());

    mResources.mTestPushConstants.mRcsBuffer = mScene.getInitialRcsBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mRisBuffer = mScene.getRisBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mBatchesBuffer = mScene.getBatchesBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mRisCount = SwRenderer::sRendererContext.mStats->mRisScratchCount.getDeviceAddress().value();
    mResources.mTestPushConstants.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mBoundsBuffer = mScene.getBoundsBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mNodeTransformsBuffer = mScene.getNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mInstancesBuffer = mScene.getInstancesBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mRisIndicesBuffer = mScene.getRisIndicesBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mRisVisibilityReadBuffer = mScene.getVisibilityRisReadBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mRisVisibilityWriteBuffer = mScene.getVisibilityRisWriteBuffer().getDeviceAddress().value();
    mResources.mTestPushConstants.mRisLimit = static_cast<std::uint32_t>(mScene.getRis().size());

    mResources.mCompactPushConstants.mBatchesBuffer = mScene.getBatchesBuffer().getDeviceAddress().value();
    mResources.mCompactPushConstants.mPreRcsBuffer = mScene.getInitialRcsBuffer().getDeviceAddress().value();
    mResources.mCompactPushConstants.mPreRcsLimit = static_cast<std::uint32_t>(mScene.getRcs().size());
}

void SwCull::System::reInitializeOnResize() {
    // PrepOcclusion
    vk::Extent3D depthImageExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D();
    vk::Extent3D depthPyramidExtent = depthImageExtent;
    depthPyramidExtent.width = SwHelper::previousPow2(depthPyramidExtent.width);
    depthPyramidExtent.height = SwHelper::previousPow2(depthPyramidExtent.height);

    mResources.mDepthPyramidLevels = SwHelper::calculateMipMapLevels(depthPyramidExtent);
    mResources.mDepthPyramidImage = SwImageFactory::createColorImage2D(
        "CullDepthPyramidImage", vk::Format::eR32Sfloat, depthPyramidExtent, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
    );
    for (std::uint32_t i = 0; i < mResources.mDepthPyramidLevels; i++) {
        mResources.mDepthPyramidImage.addImageView(
            std::format("DepthPyramidImage_Level{:0>4}", i),
            mResources.mDepthPyramidImage.getMainFormat(),
            vk::ImageAspectFlagBits::eColor,
            vk::ImageViewType::e2D,
            i,
            1
        );
    }
    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this](vk::CommandBuffer cmd) {
        mResources.mDepthPyramidImage.emitTransition(cmd, SwDependency::ImageDepType::ComputeStorageReadWrite);
    });

    mResources.mPrepOcclusionDescriptorSet.writeImage(
        0, SwRenderer::sRendererContext.mSwapchain->getDepthImage().getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    for (std::uint32_t i = 0; i < MAX_DEPTH_PYRAMID_LEVELS; i++) {
        const std::uint32_t viewIndex = std::min(i, mResources.mDepthPyramidLevels - 1);  // Write over later slots with the last level view
        mResources.mPrepOcclusionDescriptorSet.writeImage(
            1, mResources.mDepthPyramidImage.getOtherImageViewHandle(viewIndex), nullptr, vk::ImageLayout::eGeneral, i
        );
        mResources.mPrepOcclusionDescriptorSet.writeImage(
            2, mResources.mDepthPyramidImage.getOtherImageViewHandle(viewIndex), nullptr, vk::ImageLayout::eGeneral, i
        );
    }
    mResources.mPrepOcclusionDescriptorSet.pushWrites();

    // Test*
    mResources.mTestDescriptorSet.writeImage(0, mResources.mDepthPyramidImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mTestDescriptorSet.pushWrites();
}
