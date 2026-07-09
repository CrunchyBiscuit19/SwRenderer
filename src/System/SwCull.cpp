#include <Renderer/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <System/SwCull.h>
#include <quill/LogMacros.h>

#include <format>

SwCull::System::System(SwScene& scene) : SwSystem(scene) {}

void SwCull::System::initializeOtherPasses() {
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
        cmd.fillBuffer(mScene.getSceneDrawRisIndicesBuffer().getHandle(), 0, vk::WholeSize, UINT32_MAX);
        cmd.fillBuffer(mScene.getSceneVisibilityRisWriteBuffer().getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getSceneEarlyRcsBuffer().getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getSceneEarlyRcsCount().getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getSceneLateRcsBuffer().getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getSceneLateRcsCount().getHandle(), 0, vk::WholeSize, 0);

        cmd.pushConstants<SwCull::ResetPC>(mResources.mResetPipelineBundle.getLayoutHandle(), SwCull::ResetPC::sStages, 0, mResources.mResetPushConstants);

        cmd.dispatch(SwHelper::fastDivCeil(mResources.mResetPushConstants.mSceneRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // EarlyWork
    mScene.insertPass(SwPass::Type::CullEarlyWork, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getPipelineHandle());

        cmd.bindDescriptorSets(
            mResources.mWorkPipelineBundle.getBindPoint(),
            mResources.mWorkPipelineBundle.getLayoutHandle(),
            0,
            mResources.mWorkDescriptorSet.getHandle(),
            nullptr
        );

        mResources.mWorkPushConstants.mPhase = SwCull::Phase::Early;
        cmd.pushConstants<SwCull::WorkPC>(mResources.mWorkPipelineBundle.getLayoutHandle(), SwCull::WorkPC::sStages, 0, mResources.mWorkPushConstants);

        cmd.dispatch(SwHelper::fastDivCeil(mResources.mWorkPushConstants.mSceneRisLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // EarlyCompact
    mScene.insertPass(SwPass::Type::CullEarlyCompact, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getPipelineHandle());

        mResources.mCompactPushConstants.mPostRcsBuffer = mScene.getSceneEarlyRcsBuffer().getDeviceAddress().value();
        mResources.mCompactPushConstants.mPostRcsCount = mScene.getSceneEarlyRcsCount().getDeviceAddress().value();
        cmd.pushConstants<SwCull::CompactPC>(
            mResources.mCompactPipelineBundle.getLayoutHandle(), SwCull::CompactPC::sStages, 0, mResources.mCompactPushConstants
        );

        cmd.dispatch(SwHelper::fastDivCeil(mResources.mCompactPushConstants.mPreRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });
}

void SwCull::System::initializeLatePasses() {
    // LateReset
    // Zero initialRcs.mRiCount after the early compact has snapshotted it. Late draw list limited to just the newly-visible delta.
    mScene.insertPass(SwPass::Type::CullLateReset, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mResetPipelineBundle.getBindPoint(), mResources.mResetPipelineBundle.getPipelineHandle());

        cmd.pushConstants<SwCull::ResetPC>(mResources.mResetPipelineBundle.getLayoutHandle(), SwCull::ResetPC::sStages, 0, mResources.mResetPushConstants);

        cmd.dispatch(SwHelper::fastDivCeil(mScene.getSceneRcs().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

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

    // LateWork
    mScene.insertPass(SwPass::Type::CullLateWork, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getPipelineHandle());

        cmd.bindDescriptorSets(
            mResources.mWorkPipelineBundle.getBindPoint(),
            mResources.mWorkPipelineBundle.getLayoutHandle(),
            0,
            mResources.mWorkDescriptorSet.getHandle(),
            nullptr
        );

        mResources.mWorkPushConstants.mPhase = SwCull::Phase::Late;
        cmd.pushConstants<SwCull::WorkPC>(mResources.mWorkPipelineBundle.getLayoutHandle(), SwCull::WorkPC::sStages, 0, mResources.mWorkPushConstants);

        cmd.dispatch(SwHelper::fastDivCeil(mResources.mWorkPushConstants.mSceneRisLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
        // hasEarlyDraw depends on the material type now
    });

    // LateCompact
    mScene.insertPass(SwPass::Type::CullLateCompact, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getPipelineHandle());

        mResources.mCompactPushConstants.mPostRcsBuffer = mScene.getSceneLateRcsBuffer().getDeviceAddress().value();
        mResources.mCompactPushConstants.mPostRcsCount = mScene.getSceneLateRcsCount().getDeviceAddress().value();
        cmd.pushConstants<SwCull::CompactPC>(
            mResources.mCompactPipelineBundle.getLayoutHandle(), SwCull::CompactPC::sStages, 0, mResources.mCompactPushConstants
        );

        cmd.dispatch(SwHelper::fastDivCeil(mResources.mCompactPushConstants.mPreRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });
}

void SwCull::System::initializeResources() {
    // Reset*
    mResources.mResetPipelineLayout = SwPipelineFactory::createPipelineLayout("CullResetPipelineLayout", nullptr, SwCull::ResetPC::getRange());
    SwShader resetShader = SwShaderFactory::createShader("CullResetShaderModule", CULL_RESET_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mResetPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("CullResetPipeline", {resetShader.getHandle(), mResources.mResetPipelineLayout.getHandle()});

    // Compact*
    mResources.mCompactPipelineLayout = SwPipelineFactory::createPipelineLayout("CullCompactPipelineLayout", nullptr, SwCull::CompactPC::getRange());
    SwShader compactShader = SwShaderFactory::createShader("CullCompactShaderModule", CULL_COMPACT_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mCompactPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("CullCompactPipeline", {compactShader.getHandle(), mResources.mCompactPipelineLayout.getHandle()});

    // PrepOcclusion
    mResources.mPrepOcclusionDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "CullPrepOcclusionDescriptorSetLayout",
        {{0, vk::DescriptorType::eSampledImage, 1},
         {1, vk::DescriptorType::eSampledImage, CULL_MAX_DEPTH_PYRAMID_LEVELS},
         {2, vk::DescriptorType::eStorageImage, CULL_MAX_DEPTH_PYRAMID_LEVELS},
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
        SwShaderFactory::createShader("CullPrepOcclusionShaderModule", CULL_PREP_OCCLUSION_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mPrepOcclusionPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "CullPrepOcclusionPipeline", {depthPyramidShader.getHandle(), mResources.mPrepOcclusionPipelineLayout.getHandle()}
    );

    // Work*
    mResources.mWorkDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "CullWorkDescriptorSetLayout", {{0, vk::DescriptorType::eSampledImage, 1}, {1, vk::DescriptorType::eSampler, 1}}, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mWorkDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("CullWorkDescriptorSet", mResources.mWorkDescriptorLayout);

    vk::SamplerReductionModeCreateInfo workReductionInfo{vk::SamplerReductionMode::eMin};
    vk::SamplerCreateInfo workSamplerInfo{};
    workSamplerInfo.setPNext(&workReductionInfo);
    workSamplerInfo.setMagFilter(vk::Filter::eLinear);
    workSamplerInfo.setMinFilter(vk::Filter::eLinear);
    workSamplerInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
    workSamplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    workSamplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    workSamplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    workSamplerInfo.setMaxLod(VK_LOD_CLAMP_NONE);
    mResources.mWorkDepthPyramidSampler = SwSamplerFactory::createSampler("CullWorkDepthPyramidSampler", workSamplerInfo);
    mResources.mWorkDescriptorSet.writeSampler(1, mResources.mWorkDepthPyramidSampler.getHandle());

    mResources.mWorkPipelineLayout =
        SwPipelineFactory::createPipelineLayout("CullWorkPipelineLayout", mResources.mWorkDescriptorLayout.getHandle(), SwCull::WorkPC::getRange());
    SwShader workShader = SwShaderFactory::createShader("CullWorkShaderModule", CULL_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mWorkPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("CullWorkPipeline", {workShader.getHandle(), mResources.mWorkPipelineLayout.getHandle()});

    reInitializeOnResize();
}

void SwCull::System::initializePasses() {
    initializeOtherPasses();
    initializeEarlyPasses();
    initializeLatePasses();
}

void SwCull::System::refreshDependencies() {
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
        d.mWriteBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneEarlyRcsBuffer(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneEarlyRcsCount(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneLateRcsBuffer(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneLateRcsCount(), SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneVisibilityRisWriteBuffer(), SwDependency::BufferDepType::TransferWrite);
    }

    // EarlyWork
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullEarlyWork].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeShaderSampledRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneRisBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneVisibilityRisReadBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneVisibilityRisWriteBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // EarlyCompact
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullEarlyCompact].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mScene.getSceneInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mScene.getSceneEarlyRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneEarlyRcsCount(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // LateReset
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullLateReset].getDeps();
        d.clear();
        d.mWriteBuffers.emplace_back(&mScene.getSceneInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // PrepOcclusion
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullPrepOcclusion].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::ComputeShaderSampledRead);
        d.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);
        d.mWriteImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);
    }

    // LateWork
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullLateWork].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeShaderSampledRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneRisBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneVisibilityRisReadBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneVisibilityRisWriteBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // LateCompact
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::CullLateCompact].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mScene.getSceneInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mScene.getSceneLateRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneLateRcsCount(), SwDependency::BufferDepType::ComputeStorageWrite);
    }
}

void SwCull::System::refreshPushConstants() {
    mResources.mResetPushConstants.mSceneRcsBuffer = mScene.getSceneInitialRcsBuffer().getDeviceAddress().value();
    mResources.mResetPushConstants.mSceneRcsLimit = static_cast<std::uint32_t>(mScene.getSceneRcs().size());

    mResources.mWorkPushConstants.mSceneRcsBuffer = mScene.getSceneInitialRcsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneRisBuffer = mScene.getSceneRisBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mStatsRisCount = SwRenderer::sRendererContext.mStats->mRisScratchCount.getDeviceAddress().value();
    mResources.mWorkPushConstants.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneBoundsBuffer = mScene.getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneDrawRisIndicesBuffer = mScene.getSceneDrawRisIndicesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneVisibilityRisReadBuffer = mScene.getSceneVisibilityRisReadBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneVisibilityRisWriteBuffer = mScene.getSceneVisibilityRisWriteBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneRisLimit = static_cast<std::uint32_t>(mScene.getSceneRis().size());

    mResources.mCompactPushConstants.mPreRcsBuffer = mScene.getSceneInitialRcsBuffer().getDeviceAddress().value();
    mResources.mCompactPushConstants.mPreRcsLimit = static_cast<std::uint32_t>(mScene.getSceneRcs().size());
}

void SwCull::System::reInitializeOnResize() {
    // PrepOcclusion
    vk::Extent3D depthImageExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D();
    vk::Extent3D depthPyramidExtent = depthImageExtent;
    depthPyramidExtent.width = SwHelper::previousPow2(depthPyramidExtent.width);
    depthPyramidExtent.height = SwHelper::previousPow2(depthPyramidExtent.height);

    mResources.mDepthPyramidLevels = SwHelper::calculateMipMapLevels(depthPyramidExtent);
    mResources.mDepthPyramidImage = SwImageFactory::createColorImage2D(
        "DepthPyramidImage", vk::Format::eR32Sfloat, depthPyramidExtent, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
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
    SwRenderer::sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mResources.mDepthPyramidImage.emitTransition(cmd, SwDependency::ImageDepType::ComputeStorageReadWrite);
    });

    mResources.mPrepOcclusionDescriptorSet.writeImage(
        0, SwRenderer::sRendererContext.mSwapchain->getDepthImage().getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    for (std::uint32_t i = 0; i < CULL_MAX_DEPTH_PYRAMID_LEVELS; i++) {
        const std::uint32_t viewIndex = std::min(i, mResources.mDepthPyramidLevels - 1);  // Write over later slots with the last level view
        mResources.mPrepOcclusionDescriptorSet.writeImage(
            1, mResources.mDepthPyramidImage.getOtherImageViewHandle(viewIndex), nullptr, vk::ImageLayout::eGeneral, i
        );
        mResources.mPrepOcclusionDescriptorSet.writeImage(
            2, mResources.mDepthPyramidImage.getOtherImageViewHandle(viewIndex), nullptr, vk::ImageLayout::eGeneral, i
        );
    }
    mResources.mPrepOcclusionDescriptorSet.pushWrites();

    // Work*
    mResources.mWorkDescriptorSet.writeImage(0, mResources.mDepthPyramidImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mWorkDescriptorSet.pushWrites();
}
