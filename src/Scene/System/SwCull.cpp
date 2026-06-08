#include <Misc/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwCull.h>
#include <quill/LogMacros.h>

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
            SwRenderer::sRendererContext.mStats->mRisScratchCount.getRawBuffer(),
            SwRenderer::sRendererContext.mStats->mRisPublishedCount.getRawBuffer(),
            region
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
        
        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
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

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
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

            mResources.mWorkPushConstants.mRcsBuffer = batch.getEarlyRcsBuffer().getDeviceAddress().value();
            mResources.mWorkPushConstants.mRisBuffer = batch.getRisBuffer().getDeviceAddress().value();
            mResources.mWorkPushConstants.mRisLimit = batch.getRis().size();
            mResources.mWorkPushConstants.mPhase = SwCull::Phase::Late;
            cmd.pushConstants<SwCull::WorkPC>(mResources.mWorkPipelineBundle.getRawLayout(), SwCull::WorkPC::sStages, 0, mResources.mWorkPushConstants);

            cmd.dispatch(SwHelper::fastDivCeil(batch.getRis().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
        }
    });
    staticDeps.clear();

    // LateCompact
    mScene.insertPass(SwPass::Type::CullLateCompact, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getRawPipeline());

        for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
            if (batch.getRcs().empty()) {
                continue;
            }

            mResources.mCompactPushConstants.mPreRcsBuffer = batch.getEarlyRcsBuffer().getDeviceAddress().value();
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

    // Work*
    mResources.mWorkDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mWorkDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mWorkDescriptorLayout);
    mResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout(mResources.mWorkDescriptorLayout.getRawLayout(), SwCull::WorkPC::getRange());
    SwShader workShader = SwShaderFactory::createShader(CULL_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mWorkPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mResources.mWorkPipelineLayout.getRawLayout()});

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
    mResources.mPrepOcclusionPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mResources.mPrepOcclusionDescriptorLayout.getRawLayout(), SwCull::PrepOcclusionPC::getRange());
    SwShader depthPyramidShader = SwShaderFactory::createShader(CULL_PREP_OCCLUSION_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mPrepOcclusionPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({depthPyramidShader.getRawModule(), mResources.mPrepOcclusionPipelineLayout.getRawLayout()});

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

    // LateWork
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::ComputeStorageRead
    );
    dynamicDeps.mReadBuffers.emplace_back(&mScene.getSceneVisibilityRisReadBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    dynamicDeps.mWriteBuffers.emplace_back(&mScene.getSceneVisibilityRisWriteBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
        if (batch.getRcs().empty()) continue;
        dynamicDeps.mReadBuffers.emplace_back(&batch.getEarlyRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getRisBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    }
    mScene.mPasses[SwPass::Type::CullLateWork].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // LateCompact
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
        if (batch.getRcs().empty()) continue;
        dynamicDeps.mReadBuffers.emplace_back(&batch.getEarlyRcsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
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
    vk::Extent3D depthPyramidExtent = vk::Extent3D{
        SwHelper::previousPow2(SwRenderer::sRendererContext.mSwapchain->getWindowExtent().width),
        SwHelper::previousPow2(SwRenderer::sRendererContext.mSwapchain->getWindowExtent().height),
        1,
    };
    mResources.mDepthPyramidLevels = SwHelper::calculateMipMapLevels(depthPyramidExtent);
    mResources.mDepthPyramidImage = SwImageFactory::createColorImage2D(
        nullptr, vk::Format::eR32Sfloat, depthPyramidExtent, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
    );
    for (std::uint32_t i = 0; i < mResources.mDepthPyramidLevels; i++) {
        mResources.mDepthPyramidImage.addImageView(
            mResources.mDepthPyramidImage.getMainFormat(), vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, i, 1
        );
    }
    SwRenderer::sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mResources.mDepthPyramidImage.emitTransition(cmd, SwDependency::ImageDepType::ComputeStorageReadWrite);
    });

    mResources.mPrepOcclusionDescriptorSet.writeImage(
        0, SwRenderer::sRendererContext.mSwapchain->getDepthImage().getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    for (std::uint32_t i = 0; i < mResources.mDepthPyramidLevels; i++) {
        mResources.mPrepOcclusionDescriptorSet.writeImage(1, mResources.mDepthPyramidImage.getRawOtherImageView(i), nullptr, vk::ImageLayout::eGeneral, i);
        mResources.mPrepOcclusionDescriptorSet.writeImage(2, mResources.mDepthPyramidImage.getRawOtherImageView(i), nullptr, vk::ImageLayout::eGeneral, i);
    }
    mResources.mPrepOcclusionDescriptorSet.writeSampler(3, mResources.mDepthPyramidMinSampler.getRawSampler());
    mResources.mPrepOcclusionDescriptorSet.pushWrites();

    vk::Extent3D depthExtent = vk::Extent3D{SwRenderer::sRendererContext.mSwapchain->getWindowExtent(), 1};
    mResources.mPrepOcclusionPushConstants.mDepthPyramidExtent = glm::uvec2(depthPyramidExtent.width, depthPyramidExtent.height);
    mResources.mPrepOcclusionPushConstants.mDepthFullExtent = glm::uvec2(depthExtent.width, depthExtent.height);
    mResources.mPrepOcclusionPushConstants.mDepthFullRatio =
        glm::vec2(depthPyramidExtent.width / static_cast<float>(depthExtent.width), depthPyramidExtent.height / static_cast<float>(depthExtent.height));

    // Work*
    mResources.mWorkDescriptorSet.writeImage(0, mResources.mDepthPyramidImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mWorkDescriptorSet.pushWrites();

    vk::Extent2D drawExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent();
    mResources.mWorkPushConstants.mDrawExtents = glm::vec2(drawExtent.width, drawExtent.height);
    mResources.mWorkPushConstants.mDepthPyramidExtents = glm::uvec2(depthPyramidExtent.width, depthPyramidExtent.height);
}
