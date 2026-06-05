#include <Misc/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwCull.h>
#include <quill/LogMacros.h>

#include <ranges>

SwCull::System::System(SwScene& scene) : SwSystem(scene) {}

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
    reInitializeOnResize();

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
    SwShader depthPyramidShader = SwShaderFactory::createShader(CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mPrepOcclusionPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({depthPyramidShader.getRawModule(), mResources.mPrepOcclusionPipelineLayout.getRawLayout()});
}

void SwCull::System::initializePasses() {
    SwDependency staticDeps;

    // ResetFrustum
    staticDeps.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRInstsCount, SwDependency::BufferDepType::TransferWrite);
    staticDeps.mWriteBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::TransferWrite);
    staticDeps.mWriteBuffers.emplace_back(&mScene.getCamera().getFrustumBuffer(), SwDependency::BufferDepType::HostWrite);
    mScene.insertPass(SwPass::Type::CullResetFrustum, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mResetPipelineBundle.getBindPoint(), mResources.mResetPipelineBundle.getRawPipeline());
        cmd.fillBuffer(SwRenderer::sRendererContext.mStats->mRInstsCount.getRawBuffer(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getSceneVisibleRInstsIndicesBuffer().getRawBuffer(), 0, vk::WholeSize, UINT32_MAX);
        mScene.getCamera().getFrustumBuffer().copyFromUnchecked(mScene.getCamera().getFrustumPlanes().data(), SwCamera::NUM_FRUSTUM_PLANES * sizeof(Plane));
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRItems().empty()) {
                    continue;
                }
                cmd.fillBuffer(batch.getFrustumRItemsCount().getRawBuffer(), 0, vk::WholeSize, 0);
                cmd.fillBuffer(batch.getFrustumRItemsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                const std::uint32_t rItemsCount = static_cast<std::uint32_t>(batch.getRItems().size());
                mResources.mResetPushConstants.mRItemsBuffer = batch.getInitialRItemsBuffer().getDeviceAddress().value();
                mResources.mResetPushConstants.mRItemsLimit = rItemsCount;
                cmd.pushConstants<SwCull::ResetPC>(mResources.mResetPipelineBundle.getRawLayout(), SwCull::ResetPC::sStages, 0, mResources.mResetPushConstants);
                cmd.dispatch(SwHelper::fastDivCeil(rItemsCount, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    staticDeps.clear();

    // WorkFrustum
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getCamera().getFrustumBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRInstsCount, SwDependency::BufferDepType::ComputeStorageWrite);
    staticDeps.mWriteBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    mScene.insertPass(SwPass::Type::CullWorkFrustum, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawPipeline());
        mResources.mWorkPushConstants.mPerFrameBuffer =
            SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRItems().empty()) {
                    continue;
                }
                cmd.bindDescriptorSets(
                    mResources.mWorkPipelineBundle.getBindPoint(),
                    mResources.mWorkPipelineBundle.getRawLayout(),
                    0,
                    mResources.mWorkDescriptorSet.getRawSet(),
                    nullptr
                );
                mResources.mWorkPushConstants.mRItemsBuffer = batch.getInitialRItemsBuffer().getDeviceAddress().value();
                mResources.mWorkPushConstants.mRInstsBuffer = batch.getRInstsBuffer().getDeviceAddress().value();
                mResources.mWorkPushConstants.mRInstsLimit = batch.getRInsts().size();
                cmd.pushConstants<SwCull::WorkPC>(mResources.mWorkPipelineBundle.getRawLayout(), SwCull::WorkPC::sStages, 0, mResources.mWorkPushConstants);
                cmd.dispatch(SwHelper::fastDivCeil(batch.getRInsts().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    staticDeps.clear();

    // CompactFrustum
    mScene.insertPass(SwPass::Type::CullCompactFrustum, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getRawPipeline());
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRItems().empty()) {
                    continue;
                }
                mResources.mCompactPushConstants.mPreRItemsBuffer = batch.getInitialRItemsBuffer().getDeviceAddress().value();
                mResources.mCompactPushConstants.mPostRItemsBuffer = batch.getFrustumRItemsBuffer().getDeviceAddress().value();
                mResources.mCompactPushConstants.mPostRItemsCount = batch.getFrustumRItemsCount().getDeviceAddress().value();
                mResources.mCompactPushConstants.mPreRItemsLimit = batch.getRItems().size();
                cmd.pushConstants<SwCull::CompactPC>(
                    mResources.mCompactPipelineBundle.getRawLayout(), SwCull::CompactPC::sStages, 0, mResources.mCompactPushConstants
                );
                cmd.dispatch(SwHelper::fastDivCeil(batch.getRItems().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
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
            vk::PipelineBindPoint::eCompute,
            mResources.mPrepOcclusionPipelineBundle.getRawLayout(),
            0,
            mResources.mPrepOcclusionDescriptorSet.getRawSet(),
            nullptr
        );
        mResources.mPrepOcclusionPushConstants.mReadFromFull = true;
        mResources.mPrepOcclusionPushConstants.mLevel = 0;
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
        mResources.mPrepOcclusionPushConstants.mReadFromFull = false;
        for (std::uint32_t i = 0; i < mResources.mDepthPyramidLevels - 1; i++) {
            cmd.bindDescriptorSets(
                mResources.mPrepOcclusionPipelineBundle.getBindPoint(),
                mResources.mPrepOcclusionPipelineBundle.getRawLayout(),
                0,
                mResources.mPrepOcclusionDescriptorSet.getRawSet(),
                nullptr
            );
            mResources.mPrepOcclusionPushConstants.mLevel = i;
            cmd.pushConstants<SwCull::PrepOcclusionPC>(
                mResources.mPrepOcclusionPipelineBundle.getRawLayout(), SwCull::PrepOcclusionPC::sStages, 0, mResources.mPrepOcclusionPushConstants
            );
            const std::uint32_t dstW = std::max(1u, mResources.mDepthPyramidImage.getExtent().width >> (i + 1));
            const std::uint32_t dstH = std::max(1u, mResources.mDepthPyramidImage.getExtent().height >> (i + 1));
            cmd.dispatch(
                SwHelper::fastDivCeil(dstW, SwRenderer::MAX_2D_WORKGROUP_THREADS), SwHelper::fastDivCeil(dstH, SwRenderer::MAX_2D_WORKGROUP_THREADS), 1
            );
            if (i < mResources.mDepthPyramidLevels - 2) {
                mResources.mDepthPyramidImage.emitTransition(
                    cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead, vk::ImageLayout::eGeneral, i + 1
                );
            }
        }
    });
    staticDeps.clear();

    // ResetOcclusion
    staticDeps.mWriteBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::TransferWrite);
    mScene.insertPass(SwPass::Type::CullResetOcclusion, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mResetPipelineBundle.getBindPoint(), mResources.mResetPipelineBundle.getRawPipeline());
        cmd.fillBuffer(mScene.getSceneVisibleRInstsIndicesBuffer().getRawBuffer(), 0, vk::WholeSize, UINT32_MAX);
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRItems().empty()) {
                    continue;
                }
                cmd.fillBuffer(batch.getOcclusionRItemsCount().getRawBuffer(), 0, vk::WholeSize, 0);
                cmd.fillBuffer(batch.getOcclusionRItemsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                const std::uint32_t rItemsCount = static_cast<std::uint32_t>(batch.getRItems().size());  // CAUTION soft limit
                mResources.mResetPushConstants.mRItemsBuffer = batch.getFrustumRItemsBuffer().getDeviceAddress().value();
                mResources.mResetPushConstants.mRItemsLimit = rItemsCount;
                cmd.pushConstants<SwCull::ResetPC>(mResources.mResetPipelineBundle.getRawLayout(), SwCull::ResetPC::sStages, 0, mResources.mResetPushConstants);
                cmd.dispatch(SwHelper::fastDivCeil(rItemsCount, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    staticDeps.clear();

    // WorkOcclusion
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getCamera().getFrustumBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mWriteBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRInstsCount, SwDependency::BufferDepType::ComputeStorageWrite);
    staticDeps.mWriteBuffers.emplace_back(&mScene.getSceneVisibleRInstsIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    mScene.insertPass(SwPass::Type::CullWorkFrustum, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawPipeline());
        mResources.mWorkPushConstants.mPerFrameBuffer =
            SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRItems().empty()) {
                    continue;
                }
                cmd.bindDescriptorSets(
                    mResources.mWorkPipelineBundle.getBindPoint(),
                    mResources.mWorkPipelineBundle.getRawLayout(),
                    0,
                    mResources.mWorkDescriptorSet.getRawSet(),
                    nullptr
                );
                mResources.mWorkPushConstants.mRItemsBuffer = batch.getFrustumRItemsBuffer().getDeviceAddress().value();
                mResources.mWorkPushConstants.mRInstsBuffer = batch.getRInstsBuffer().getDeviceAddress().value();
                mResources.mWorkPushConstants.mRInstsLimit = batch.getRInsts().size();
                cmd.pushConstants<SwCull::WorkPC>(mResources.mWorkPipelineBundle.getRawLayout(), SwCull::WorkPC::sStages, 0, mResources.mWorkPushConstants);
                cmd.dispatch(SwHelper::fastDivCeil(batch.getRInsts().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    staticDeps.clear();

    // CompactOcclusion
    mScene.insertPass(SwPass::Type::CullCompactFrustum, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getRawPipeline());
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRItems().empty()) {
                    continue;
                }
                mResources.mCompactPushConstants.mPreRItemsBuffer = batch.getFrustumRItemsBuffer().getDeviceAddress().value();
                mResources.mCompactPushConstants.mPostRItemsBuffer = batch.getOcclusionRItemsBuffer().getDeviceAddress().value();
                mResources.mCompactPushConstants.mPostRItemsCount = batch.getOcclusionRItemsCount().getDeviceAddress().value();
                mResources.mCompactPushConstants.mPreRItemsLimit = batch.getRItems().size();
                cmd.pushConstants<SwCull::CompactPC>(
                    mResources.mCompactPipelineBundle.getRawLayout(), SwCull::CompactPC::sStages, 0, mResources.mCompactPushConstants
                );
                cmd.dispatch(SwHelper::fastDivCeil(batch.getRItems().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    staticDeps.clear();
}

void SwCull::System::initializePushConstants() {
    mResources.mWorkPushConstants.mRInstsCount = SwRenderer::sRendererContext.mStats->mRInstsCount.getDeviceAddress().value();
}

void SwCull::System::refreshDynamicDependencies() {
    SwDependency dynamicDeps;

    // ResetFrustum
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRItems().empty()) continue;
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getInitialRItemsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getFrustumRItemsBuffer(), SwDependency::BufferDepType::TransferWrite);
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getFrustumRItemsCount(), SwDependency::BufferDepType::TransferWrite);
        }
    }
    mScene.mPasses[SwPass::Type::CullResetFrustum].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // WorkFrustum
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::ComputeStorageRead
    );
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRItems().empty()) continue;
            dynamicDeps.mReadBuffers.emplace_back(&batch.getInitialRItemsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
            dynamicDeps.mReadBuffers.emplace_back(&batch.getRInstsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        }
    }
    mScene.mPasses[SwPass::Type::CullWorkFrustum].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // CompactFrustum
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRItems().empty()) continue;
            dynamicDeps.mReadBuffers.emplace_back(&batch.getInitialRItemsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getFrustumRItemsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getFrustumRItemsCount(), SwDependency::BufferDepType::ComputeStorageWrite);
        }
    }
    mScene.mPasses[SwPass::Type::CullCompactFrustum].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // PrepOcclusion

    // ResetOcclusion
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRItems().empty()) continue;
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getFrustumRItemsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getOcclusionRItemsBuffer(), SwDependency::BufferDepType::TransferWrite);
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getOcclusionRItemsCount(), SwDependency::BufferDepType::TransferWrite);
        }
    }
    mScene.mPasses[SwPass::Type::CullResetFrustum].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // WorkOcclusion
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::ComputeStorageRead
    );
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRItems().empty()) continue;
            dynamicDeps.mReadBuffers.emplace_back(&batch.getFrustumRItemsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
            dynamicDeps.mReadBuffers.emplace_back(&batch.getRInstsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        }
    }
    mScene.mPasses[SwPass::Type::CullWorkFrustum].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    // CompactOcclusion
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRItems().empty()) continue;
            dynamicDeps.mReadBuffers.emplace_back(&batch.getFrustumRItemsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getOcclusionRItemsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
            dynamicDeps.mWriteBuffers.emplace_back(&batch.getOcclusionRItemsCount(), SwDependency::BufferDepType::ComputeStorageWrite);
        }
    }
    mScene.mPasses[SwPass::Type::CullCompactFrustum].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();
}

void SwCull::System::refreshPushConstants() {
    mResources.mWorkPushConstants.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneBoundsBuffer = mScene.getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mFrustumBuffer = mScene.getCamera().getFrustumBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneVisibleRInstsIndicesBuffer = mScene.getSceneVisibleRInstsIndicesBuffer().getDeviceAddress().value();
}

void SwCull::System::reInitializeOnResize() {
    // PrepOcclusion
    mResources.mDepthPyramidLevels = SwHelper::calculateMipMapLevels(mResources.mDepthPyramidImage.getExtent());
    mResources.mDepthPyramidImage = SwImageFactory::createColorImage2D(
        nullptr, vk::Format::eR32Sfloat, mResources.mDepthPyramidImage.getExtent(), vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
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

    vk::Extent3D depthPyramidExtent = mResources.mDepthPyramidImage.getExtent();
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