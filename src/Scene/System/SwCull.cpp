#include <Misc/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwCull.h>
#include <quill/LogMacros.h>
#include <fmt/core.h>

SwCull::System::System(SwScene& scene) : SwSystem(scene) {}

void SwCull::System::initializeOtherPasses() {
    SwDependency staticDeps;

    // Reset
    staticDeps.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::TransferWrite);
    staticDeps.mWriteBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::TransferWrite);
    staticDeps.mWriteBuffers.emplace_back(&mScene.getCamera().getFrustumBuffer(), SwDependency::BufferDepType::HostWrite);

    mScene.insertPass(SwPass::Type::CullReset, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mResetPipelineBundle.getBindPoint(), mResources.mResetPipelineBundle.getRawPipeline());

        cmd.fillBuffer(SwRenderer::sRendererContext.mStats->mRisScratchCount.getRawBuffer(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getSceneDrawRisIndicesBuffer().getRawBuffer(), 0, vk::WholeSize, UINT32_MAX);
        mScene.getCamera().getFrustumBuffer().copyFromUnchecked(mScene.getCamera().getFrustumPlanes().data(), SwCamera::NUM_FRUSTUM_PLANES * sizeof(Plane));
        cmd.fillBuffer(mScene.getSceneVisibilityRisWriteBuffer().getRawBuffer(), 0, vk::WholeSize, 0);

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
            if (batch.getRcs().empty()) {
                continue;
            }

            cmd.fillBuffer(batch.getEarlyRcsCount().getRawBuffer(), 0, vk::WholeSize, 0);
            cmd.fillBuffer(batch.getEarlyRcsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
            cmd.fillBuffer(batch.getLateRcsCount().getRawBuffer(), 0, vk::WholeSize, 0);
            cmd.fillBuffer(batch.getLateRcsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);

            const std::uint32_t rcsCount = static_cast<std::uint32_t>(batch.getRcs().size());
            mResources.mResetPushConstants.mRcsBuffer = batch.getInitialRcsBuffer().getDeviceAddress().value();
            mResources.mResetPushConstants.mRcsLimit = rcsCount;
            cmd.pushConstants<SwCull::ResetPC>(mResources.mResetPipelineBundle.getRawLayout(), SwCull::ResetPC::sStages, 0, mResources.mResetPushConstants);

            cmd.dispatch(SwHelper::fastDivCeil(rcsCount, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
        }
    });
    staticDeps.clear();

    // PublishCount
    staticDeps.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::TransferRead);
    staticDeps.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisPublishedCount, SwDependency::BufferDepType::TransferWrite);

    mScene.insertPass(SwPass::Type::CullPublishCount, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        const vk::BufferCopy region{0, 0, sizeof(std::uint32_t)};
        cmd.copyBuffer(
            SwRenderer::sRendererContext.mStats->mRisScratchCount.getRawBuffer(), SwRenderer::sRendererContext.mStats->mRisPublishedCount.getRawBuffer(), region
        );
    });
    staticDeps.clear();
}

void SwCull::System::initializeEarlyPasses() {
    SwDependency staticDeps;

    // EarlyWork
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getCamera().getFrustumBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::ComputeStorageWrite);
    staticDeps.mWriteBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);

    mScene.insertPass(SwPass::Type::CullEarlyWork, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawPipeline());

        // Only opaque has an early geometry pass; masked/transparent are handled entirely in the late pass.
        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque)) {
            if (batch.getRcs().empty()) {
                continue;
            }

            cmd.bindDescriptorSets(
                mResources.mWorkPipelineBundle.getBindPoint(),
                mResources.mWorkPipelineBundle.getRawLayout(),
                0,
                mResources.mWorkDescriptorSet.getRawSet(),
                nullptr
            );

            mResources.mWorkPushConstants.mRcsBuffer = batch.getInitialRcsBuffer().getDeviceAddress().value();
            mResources.mWorkPushConstants.mRisBuffer = batch.getRisBuffer().getDeviceAddress().value();
            mResources.mWorkPushConstants.mRisLimit = batch.getRis().size();
            mResources.mWorkPushConstants.mPhase = SwCull::Phase::Early;
            cmd.pushConstants<SwCull::WorkPC>(mResources.mWorkPipelineBundle.getRawLayout(), SwCull::WorkPC::sStages, 0, mResources.mWorkPushConstants);

            cmd.dispatch(SwHelper::fastDivCeil(batch.getRis().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
        }
    });
    staticDeps.clear();

    // EarlyCompact
    mScene.insertPass(SwPass::Type::CullEarlyCompact, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getRawPipeline());

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
            if (batch.getRcs().empty()) {
                continue;
            }

            mResources.mCompactPushConstants.mPreRcsBuffer = batch.getInitialRcsBuffer().getDeviceAddress().value();
            mResources.mCompactPushConstants.mPostRcsBuffer = batch.getEarlyRcsBuffer().getDeviceAddress().value();
            mResources.mCompactPushConstants.mPostRcsCount = batch.getEarlyRcsCount().getDeviceAddress().value();
            mResources.mCompactPushConstants.mPreRcsLimit = batch.getRcs().size();
            cmd.pushConstants<SwCull::CompactPC>(
                mResources.mCompactPipelineBundle.getRawLayout(), SwCull::CompactPC::sStages, 0, mResources.mCompactPushConstants
            );

            cmd.dispatch(SwHelper::fastDivCeil(batch.getRcs().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
        }
    });
    staticDeps.clear();
}

void SwCull::System::initializeLatePasses() {
    SwDependency staticDeps;

    // LateReset
    // Zero initialRcs.mRiCount after the early compact has snapshotted it, so the late work pass
    // restarts its per-rc fill at 0. This keeps the late draw list to just the newly-visible delta
    // (rather than re-emitting everything the early pass already drew).
    mScene.insertPass(SwPass::Type::CullLateReset, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mResetPipelineBundle.getBindPoint(), mResources.mResetPipelineBundle.getRawPipeline());

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
            if (batch.getRcs().empty()) {
                continue;
            }

            const std::uint32_t rcsCount = static_cast<std::uint32_t>(batch.getRcs().size());
            mResources.mResetPushConstants.mRcsBuffer = batch.getInitialRcsBuffer().getDeviceAddress().value();
            mResources.mResetPushConstants.mRcsLimit = rcsCount;
            cmd.pushConstants<SwCull::ResetPC>(mResources.mResetPipelineBundle.getRawLayout(), SwCull::ResetPC::sStages, 0, mResources.mResetPushConstants);

            cmd.dispatch(SwHelper::fastDivCeil(rcsCount, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
        }
    });
    staticDeps.clear();

    // PrepOcclusion
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::ComputeShaderSampledRead);
    staticDeps.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);
    staticDeps.mWriteImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);

    mScene.insertPass(SwPass::Type::CullPrepOcclusion, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mPrepOcclusionPipelineBundle.getBindPoint(), mResources.mPrepOcclusionPipelineBundle.getRawPipeline());

        cmd.bindDescriptorSets(
            mResources.mPrepOcclusionPipelineBundle.getBindPoint(),
            mResources.mPrepOcclusionPipelineBundle.getRawLayout(),
            0,
            mResources.mPrepOcclusionDescriptorSet.getRawSet(),
            nullptr
        );

        mResources.mPrepOcclusionPushConstants.mLevel = -1;
        cmd.pushConstants<SwCull::PrepOcclusionPC>(
            mResources.mPrepOcclusionPipelineBundle.getRawLayout(), SwCull::PrepOcclusionPC::sStages, 0, mResources.mPrepOcclusionPushConstants
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
                mResources.mPrepOcclusionPipelineBundle.getRawLayout(), SwCull::PrepOcclusionPC::sStages, 0, mResources.mPrepOcclusionPushConstants
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
    staticDeps.clear();

    // LateWork
    staticDeps.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeShaderSampledRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getCamera().getFrustumBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRisScratchCount, SwDependency::BufferDepType::ComputeStorageWrite);
    staticDeps.mWriteBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);

    mScene.insertPass(SwPass::Type::CullLateWork, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawPipeline());

        // Opaque batches are partially drawn by the early geometry pass, so the late pass only emits
        // the newly-visible delta (mHasEarlyDraw = 1). Masked/transparent have no early geometry pass
        // and are drawn solely from the late list, so they must emit the full visible set
        // (mHasEarlyDraw = 0).
        auto dispatchBatches = [&](auto&& batches, bool hasEarlyDraw) {
            for (auto& batch : batches) {
                if (batch.getRcs().empty()) {
                    continue;
                }

                cmd.bindDescriptorSets(
                    mResources.mWorkPipelineBundle.getBindPoint(),
                    mResources.mWorkPipelineBundle.getRawLayout(),
                    0,
                    mResources.mWorkDescriptorSet.getRawSet(),
                    nullptr
                );

                mResources.mWorkPushConstants.mRcsBuffer = batch.getInitialRcsBuffer().getDeviceAddress().value();
                mResources.mWorkPushConstants.mRisBuffer = batch.getRisBuffer().getDeviceAddress().value();
                mResources.mWorkPushConstants.mRisLimit = batch.getRis().size();
                mResources.mWorkPushConstants.mPhase = SwCull::Phase::Late;
                mResources.mWorkPushConstants.mHasEarlyDraw = hasEarlyDraw;
                cmd.pushConstants<SwCull::WorkPC>(mResources.mWorkPipelineBundle.getRawLayout(), SwCull::WorkPC::sStages, 0, mResources.mWorkPushConstants);

                cmd.dispatch(SwHelper::fastDivCeil(batch.getRis().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        };

        dispatchBatches(mScene.getBatchIt(SwMaterial::Type::Opaque), true);
        dispatchBatches(mScene.getBatchIt(SwMaterial::Type::Mask, SwMaterial::Type::Transparent), false);
    });
    staticDeps.clear();

    // LateCompact
    mScene.insertPass(SwPass::Type::CullLateCompact, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getRawPipeline());

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
            if (batch.getRcs().empty()) {
                continue;
            }

            mResources.mCompactPushConstants.mPreRcsBuffer = batch.getInitialRcsBuffer().getDeviceAddress().value();
            mResources.mCompactPushConstants.mPostRcsBuffer = batch.getLateRcsBuffer().getDeviceAddress().value();
            mResources.mCompactPushConstants.mPostRcsCount = batch.getLateRcsCount().getDeviceAddress().value();
            mResources.mCompactPushConstants.mPreRcsLimit = batch.getRcs().size();
            cmd.pushConstants<SwCull::CompactPC>(
                mResources.mCompactPipelineBundle.getRawLayout(), SwCull::CompactPC::sStages, 0, mResources.mCompactPushConstants
            );

            cmd.dispatch(SwHelper::fastDivCeil(batch.getRcs().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
        }
    });
    staticDeps.clear();
}

void SwCull::System::initializeResources() {
    // Reset*
    mResources.mResetPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, SwCull::ResetPC::getRange());
    SwShader resetShader = SwShaderFactory::createShader(CULL_RESET_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mResetPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({resetShader.getRawModule(), mResources.mResetPipelineLayout.getRawLayout()});

    // Compact*
    mResources.mCompactPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, SwCull::CompactPC::getRange());
    SwShader compactShader = SwShaderFactory::createShader(CULL_COMPACT_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mCompactPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({compactShader.getRawModule(), mResources.mCompactPipelineLayout.getRawLayout()});

    // PrepOcclusion
    mResources.mPrepOcclusionDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
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
    mResources.mDepthPyramidMinSampler = SwSamplerFactory::createSampler(minSamplerInfo);

    mResources.mPrepOcclusionDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mPrepOcclusionDescriptorLayout);
    mResources.mPrepOcclusionDescriptorSet.writeSampler(3, mResources.mDepthPyramidMinSampler.getRawSampler());

    mResources.mPrepOcclusionPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mResources.mPrepOcclusionDescriptorLayout.getRawLayout(), SwCull::PrepOcclusionPC::getRange());
    SwShader depthPyramidShader = SwShaderFactory::createShader(CULL_PREP_OCCLUSION_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mPrepOcclusionPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({depthPyramidShader.getRawModule(), mResources.mPrepOcclusionPipelineLayout.getRawLayout()});

    // Work*
    mResources.mWorkDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1}, {1, vk::DescriptorType::eSampler, 1}}, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mWorkDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mWorkDescriptorLayout);

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
    mResources.mWorkDepthPyramidSampler = SwSamplerFactory::createSampler(workSamplerInfo);
    mResources.mWorkDescriptorSet.writeSampler(1, mResources.mWorkDepthPyramidSampler.getRawSampler());

    mResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout(mResources.mWorkDescriptorLayout.getRawLayout(), SwCull::WorkPC::getRange());
    SwShader workShader = SwShaderFactory::createShader(CULL_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mWorkPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mResources.mWorkPipelineLayout.getRawLayout()});

    reInitializeOnResize();
}

void SwCull::System::initializePasses() {
    initializeOtherPasses();
    initializeEarlyPasses();
    initializeLatePasses();
}

void SwCull::System::initializePushConstants() {
    mResources.mWorkPushConstants.mRisCount = SwRenderer::sRendererContext.mStats->mRisScratchCount.getDeviceAddress().value();
}

void SwCull::System::refreshOtherDynamicDependencies() {
    SwDependency dynamicDeps;

    // Reset
    dynamicDeps.mWriteBuffers.emplace_back(&mScene.getSceneVisibilityRisWriteBuffer(), SwDependency::BufferDepType::TransferWrite);
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
        if (batch.getRcs().empty()) continue;
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getEarlyRcsBuffer(), SwDependency::BufferDepType::TransferWrite);
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getEarlyRcsCount(), SwDependency::BufferDepType::TransferWrite);
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getLateRcsBuffer(), SwDependency::BufferDepType::TransferWrite);
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getLateRcsCount(), SwDependency::BufferDepType::TransferWrite);
    }
    mScene.mPasses[SwPass::Type::CullReset].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();
}

void SwCull::System::refreshEarlyDynamicDependencies() {
    SwDependency dynamicDeps;

    // EarlyWork
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::ComputeStorageRead
    );
    dynamicDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibilityRisReadBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    dynamicDeps.mWriteBuffers.emplace_back(&mScene.getSceneVisibilityRisWriteBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
        if (batch.getRcs().empty()) continue;
        dynamicDeps.mReadBuffers.emplace_back(&batch.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getRisBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    }
    mScene.mPasses[SwPass::Type::CullEarlyWork].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // EarlyCompact
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
        if (batch.getRcs().empty()) continue;
        dynamicDeps.mReadBuffers.emplace_back(&batch.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getEarlyRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getEarlyRcsCount(), SwDependency::BufferDepType::ComputeStorageWrite);
    }
    mScene.mPasses[SwPass::Type::CullEarlyCompact].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();
}

void SwCull::System::refreshLateDynamicDependencies() {
    SwDependency dynamicDeps;

    // LateReset
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
        if (batch.getRcs().empty()) continue;
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    }
    mScene.mPasses[SwPass::Type::CullLateReset].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // LateWork
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::ComputeStorageRead
    );
    dynamicDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibilityRisReadBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    dynamicDeps.mWriteBuffers.emplace_back(&mScene.getSceneVisibilityRisWriteBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
        if (batch.getRcs().empty()) continue;
        dynamicDeps.mReadBuffers.emplace_back(&batch.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getRisBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    }
    mScene.mPasses[SwPass::Type::CullLateWork].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // LateCompact
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
        if (batch.getRcs().empty()) continue;
        dynamicDeps.mReadBuffers.emplace_back(&batch.getInitialRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getLateRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        dynamicDeps.mWriteBuffers.emplace_back(&batch.getLateRcsCount(), SwDependency::BufferDepType::ComputeStorageWrite);
    }
    mScene.mPasses[SwPass::Type::CullLateCompact].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();
}

void SwCull::System::refreshDynamicDependencies() {
    refreshOtherDynamicDependencies();
    refreshEarlyDynamicDependencies();
    refreshLateDynamicDependencies();
}

void SwCull::System::refreshPushConstants() {
    mResources.mWorkPushConstants.mFrustumBuffer = mScene.getCamera().getFrustumBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneBoundsBuffer = mScene.getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneDrawRisIndicesBuffer = mScene.getSceneDrawRisIndicesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneVisibilityRisReadBuffer = mScene.getSceneVisibilityRisReadBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneVisibilityRisWriteBuffer = mScene.getSceneVisibilityRisWriteBuffer().getDeviceAddress().value();
}

void SwCull::System::refresh() {
    mScene.toggleSceneVisibilityRisBuffer();
    SwSystem::refresh();
}

void SwCull::System::reInitializeOnResize() {
    // PrepOcclusion
    vk::Extent3D depthImageExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D();
    vk::Extent3D depthPyramidExtent = depthImageExtent;
    depthPyramidExtent.width = SwHelper::previousPow2(depthPyramidExtent.width);
    depthPyramidExtent.height = SwHelper::previousPow2(depthPyramidExtent.height);
    
    mResources.mDepthPyramidLevels = SwHelper::calculateMipMapLevels(depthPyramidExtent);
    mResources.mDepthPyramidImage = SwImageFactory::createColorImage2D(
        "DepthPyramidImage",
        nullptr, vk::Format::eR32Sfloat, depthPyramidExtent, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
    );
    for (std::uint32_t i = 0; i < mResources.mDepthPyramidLevels; i++) {
        mResources.mDepthPyramidImage.addImageView(
            fmt::format("DepthPyramidImage_Level{:0>4}", i),
            mResources.mDepthPyramidImage.getMainFormat(), vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, i, 1
        );
    }
    SwRenderer::sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mResources.mDepthPyramidImage.emitTransition(cmd, SwDependency::ImageDepType::ComputeStorageReadWrite);
    });

    mResources.mPrepOcclusionDescriptorSet.writeImage(
        0, SwRenderer::sRendererContext.mSwapchain->getDepthImage().getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    for (std::uint32_t i = 0; i < CULL_MAX_DEPTH_PYRAMID_LEVELS; i++) {
        const std::uint32_t viewIndex = std::min(i, mResources.mDepthPyramidLevels - 1); // Write over later slots with the last level view
        mResources.mPrepOcclusionDescriptorSet.writeImage(
            1, mResources.mDepthPyramidImage.getRawOtherImageView(viewIndex), nullptr, vk::ImageLayout::eGeneral, i
        );
        mResources.mPrepOcclusionDescriptorSet.writeImage(
            2, mResources.mDepthPyramidImage.getRawOtherImageView(viewIndex), nullptr, vk::ImageLayout::eGeneral, i
        );
    }
    mResources.mPrepOcclusionDescriptorSet.pushWrites();

    mResources.mPrepOcclusionPushConstants.mDepthPyramidExtent = glm::uvec2(depthPyramidExtent.width, depthPyramidExtent.height);
    mResources.mPrepOcclusionPushConstants.mDepthFullExtent = glm::uvec2(depthImageExtent.width, depthImageExtent.height);
    mResources.mPrepOcclusionPushConstants.mDepthFullRatio =
        glm::vec2(depthPyramidExtent.width / static_cast<float>(depthImageExtent.width), depthPyramidExtent.height / static_cast<float>(depthImageExtent.height));

    // Work*
    mResources.mWorkDescriptorSet.writeImage(0, mResources.mDepthPyramidImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mWorkDescriptorSet.pushWrites();
    vk::Extent2D drawExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D();
    mResources.mWorkPushConstants.mDrawExtents = glm::vec2(drawExtent.width, drawExtent.height);
    mResources.mWorkPushConstants.mDepthPyramidExtents = glm::uvec2(depthPyramidExtent.width, depthPyramidExtent.height);
}
