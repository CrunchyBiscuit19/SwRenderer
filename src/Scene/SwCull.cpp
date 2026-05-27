#include <Misc/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/SwCull.h>

#include <ranges>

SwCull::System::System(SwScene& scene): SwSystem(scene) {}

void SwCull::System::initializeResources() {
    // Reset pass
    mResources.mResetPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, SwCull::ResetPC::getRange());

    SwShader resetShader = SwShaderFactory::createShader(CULL_RESET_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mResetPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({resetShader.getRawModule(), mResources.mResetPipelineLayout.getRawLayout()});

    // Depth pyramid pass
    mResources.mDepthPyramidDescriptorLayout = sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1},
         {1, vk::DescriptorType::eSampledImage, CULL_MAX_DEPTH_PYRAMID_LEVELS},
         {2, vk::DescriptorType::eStorageImage, CULL_MAX_DEPTH_PYRAMID_LEVELS}},
        vk::ShaderStageFlagBits::eCompute
    );

    mResources.mDepthPyramidDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mDepthPyramidDescriptorLayout);

    mResources.mDepthPyramidPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mResources.mDepthPyramidDescriptorLayout.getRawLayout(), SwCull::DepthPyramidPC::getRange());

    SwShader depthPyramidShader = SwShaderFactory::createShader(CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mDepthPyramidPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({depthPyramidShader.getRawModule(), mResources.mDepthPyramidPipelineLayout.getRawLayout()});

    // Work pass
    mResources.mWorkDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute);

    mResources.mWorkDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mWorkDescriptorLayout);

    mResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout(mResources.mWorkDescriptorLayout.getRawLayout(), SwCull::WorkPC::getRange());

    SwShader workShader = SwShaderFactory::createShader(CULL_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mWorkPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mResources.mWorkPipelineLayout.getRawLayout()});

    mResources.mWorkPushConstants.mRenderInstancesCountBuffer = sRendererContext.mStats->mRenderInstancesCountBuffer.getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneBoundsBuffer = mScene.getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mFrustumBuffer = mScene.getCamera().getFrustumBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mWorkPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer().getDeviceAddress().value();

    // Compact pass
    mResources.mCompactPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, SwCull::CompactPC::getRange());

    SwShader compactShader = SwShaderFactory::createShader(CULL_COMPACT_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mCompactPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({compactShader.getRawModule(), mResources.mCompactPipelineLayout.getRawLayout()});

    // Everything that needs to be re-built on resize
    reInitializeOnResize();
}

void SwCull::System::initializePasses() {
    SwDependency deps;

    // Cull Reset
    deps.mWriteBuffers.emplace_back(&sRendererContext.mStats->mRenderInstancesCountBuffer, SwDependency::BufferDepType::TransferWrite);
    deps.mWriteBuffers.emplace_back(&mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer(), SwDependency::BufferDepType::TransferWrite);
    deps.mWriteBuffers.emplace_back(&mScene.getCamera().getFrustumBuffer(), SwDependency::BufferDepType::HostWrite);
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRenderItems().empty()) {
                continue;
            }
            deps.mWriteBuffers.emplace_back(&batch.getPostCullRenderItemsCountBuffer(), SwDependency::BufferDepType::TransferWrite);
            deps.mWriteBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::TransferWrite);
            deps.mWriteBuffers.emplace_back(&batch.getPreCullRenderItemsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        }
    }

    mScene.insertPass(SwPass::Type::CullReset, std::move(deps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mResetPipelineBundle.getBindPoint(), mResources.mResetPipelineBundle.getRawPipeline());
        cmd.fillBuffer(sRendererContext.mStats->mRenderInstancesCountBuffer.getRawBuffer(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer().getRawBuffer(), 0, vk::WholeSize, UINT32_MAX);
        std::memcpy(
            mScene.getCamera().getFrustumBuffer().getMappedPointer(), mScene.getCamera().getFrustumPlanes().data(), SwCamera::NUM_FRUSTUM_PLANES * sizeof(Plane)
        );
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                cmd.fillBuffer(batch.getPostCullRenderItemsCountBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                cmd.fillBuffer(batch.getPostCullRenderItemsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                const std::uint32_t renderItemsCount = static_cast<std::uint32_t>(batch.getRenderItems().size());
                mResources.mResetPushConstants.mPreCullRenderItemsBuffer = batch.getPreCullRenderItemsBuffer().getDeviceAddress().value();
                mResources.mResetPushConstants.mPreCullRenderItemsLimit = renderItemsCount;
                cmd.pushConstants<SwCull::ResetPC>(mResources.mResetPipelineBundle.getRawLayout(), SwCull::ResetPC::sStages, 0, mResources.mResetPushConstants);
                cmd.dispatch(SwHelper::fastDivCeil(renderItemsCount, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    deps.clear();

    

    // Cull Depth Pyramid
    deps.mReadImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::ComputeStorageRead);
    deps.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);
    deps.mWriteImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);

    mScene.insertPass(SwPass::Type::CullDepthPyramid, std::move(deps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mDepthPyramidPipelineBundle.getBindPoint(), mResources.mDepthPyramidPipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            mResources.mDepthPyramidPipelineBundle.getRawLayout(),
            0,
            mResources.mDepthPyramidDescriptorSet.getRawSet(),
            nullptr
        );
        mResources.mDepthPyramidPushConstants.mReadFromFull = true;
        mResources.mDepthPyramidPushConstants.mLevel = 0;
        cmd.pushConstants<SwCull::DepthPyramidPC>(
            mResources.mDepthPyramidPipelineBundle.getRawLayout(), SwCull::DepthPyramidPC::sStages, 0, mResources.mDepthPyramidPushConstants
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(sRendererContext.mSwapchain->getDepthImage().getExtent().width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(sRendererContext.mSwapchain->getDepthImage().getExtent().height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
        mResources.mDepthPyramidPushConstants.mReadFromFull = false;
        for (std::uint32_t i = 0; i < mResources.mDepthPyramidLevels - 1; i++) {
            cmd.bindDescriptorSets(
                mResources.mDepthPyramidPipelineBundle.getBindPoint(),
                mResources.mDepthPyramidPipelineBundle.getRawLayout(),
                0,
                mResources.mDepthPyramidDescriptorSet.getRawSet(),
                nullptr
            );
            mResources.mDepthPyramidPushConstants.mLevel = i;
            cmd.pushConstants<SwCull::DepthPyramidPC>(
                mResources.mDepthPyramidPipelineBundle.getRawLayout(), SwCull::DepthPyramidPC::sStages, 0, mResources.mDepthPyramidPushConstants
            );
            mResources.mDepthPyramidImage.emitBarrier(
                cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
            );
            cmd.dispatch(
                SwHelper::fastDivCeil(mResources.mDepthPyramidImage.getExtent().width >> i, SwRenderer::MAX_2D_WORKGROUP_THREADS),
                SwHelper::fastDivCeil(mResources.mDepthPyramidImage.getExtent().height >> i, SwRenderer::MAX_2D_WORKGROUP_THREADS),
                1
            );
        }
    });
    deps.clear();

    // Cull Work
    deps.mReadImages.emplace_back(&mResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getCamera().getFrustumBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    deps.mWriteBuffers.emplace_back(&sRendererContext.mStats->mRenderInstancesCountBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    deps.mWriteBuffers.emplace_back(&mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRenderItems().empty()) {
                continue;
            }
            deps.mReadBuffers.emplace_back(&batch.getRenderInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
            deps.mReadBuffers.emplace_back(&batch.getPreCullRenderItemsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        }
    }

    mScene.insertPass(SwPass::Type::CullWork, std::move(deps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawPipeline());
        mResources.mWorkPushConstants.mPerFrameBuffer = sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                cmd.bindDescriptorSets(
                    mResources.mWorkPipelineBundle.getBindPoint(),
                    mResources.mWorkPipelineBundle.getRawLayout(),
                    0,
                    mResources.mWorkDescriptorSet.getRawSet(),
                    nullptr
                );
                mResources.mWorkPushConstants.mPreCullRenderItemsBuffer = batch.getPreCullRenderItemsBuffer().getDeviceAddress().value();
                mResources.mWorkPushConstants.mRenderInstancesBuffer = batch.getRenderInstancesBuffer().getDeviceAddress().value();
                mResources.mWorkPushConstants.mRenderInstancesLimit = batch.getRenderInstances().size();
                cmd.pushConstants<SwCull::WorkPC>(mResources.mWorkPipelineBundle.getRawLayout(), SwCull::WorkPC::sStages, 0, mResources.mWorkPushConstants);
                cmd.dispatch(SwHelper::fastDivCeil(batch.getRenderInstances().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    deps.clear();

    // Cull Compact
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRenderItems().empty()) {
                continue;
            }
            deps.mReadBuffers.emplace_back(&batch.getPreCullRenderItemsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
            deps.mWriteBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
            deps.mWriteBuffers.emplace_back(&batch.getPostCullRenderItemsCountBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        }
    }

    mScene.insertPass(SwPass::Type::CullCompact, std::move(deps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mCompactPipelineBundle.getBindPoint(), mResources.mCompactPipelineBundle.getRawPipeline());
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                mResources.mCompactPushConstants.mPreCullRenderItemsBuffer = batch.getPreCullRenderItemsBuffer().getDeviceAddress().value();
                mResources.mCompactPushConstants.mPostCullRenderItemsBuffer = batch.getPostCullRenderItemsBuffer().getDeviceAddress().value();
                mResources.mCompactPushConstants.mPostCullRenderItemsCountBuffer = batch.getPostCullRenderItemsCountBuffer().getDeviceAddress().value();
                mResources.mCompactPushConstants.mPreCullRenderItemsLimit = batch.getRenderItems().size();
                cmd.pushConstants<SwCull::CompactPC>(
                    mResources.mCompactPipelineBundle.getRawLayout(), SwCull::CompactPC::sStages, 0, mResources.mCompactPushConstants
                );
                cmd.dispatch(SwHelper::fastDivCeil(batch.getRenderItems().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    deps.clear();
}

void SwCull::System::reInitializeOnResize() {
    // Depth pyramid pass
    mResources.mDepthPyramidExtent = vk::Extent3D{
        SwHelper::previousPow2(sRendererContext.mSwapchain->getWindowExtent().width),
        SwHelper::previousPow2(sRendererContext.mSwapchain->getWindowExtent().height),
        1,
    };
    mResources.mDepthPyramidLevels = SwHelper::calculateMipMapLevels(mResources.mDepthPyramidExtent);
    mResources.mDepthPyramidImage = SwImageFactory::createColorImage2D(
        nullptr, vk::Format::eR32Sfloat, mResources.mDepthPyramidExtent, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
    );
    for (std::uint32_t i = 0; i < mResources.mDepthPyramidLevels; i++) {
        mResources.mDepthPyramidImage.addImageView(
            mResources.mDepthPyramidImage.getMainFormat(), vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, i, 1
        );
    }
    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mResources.mDepthPyramidImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
        );
    });

    mResources.mDepthPyramidDescriptorSet.writeImage(
        0,
        sRendererContext.mSwapchain->getDepthImage().getRawMainImageView(),
        nullptr,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::DescriptorType::eSampledImage
    );
    for (std::uint32_t i = 0; i < mResources.mDepthPyramidLevels; i++) {
        mResources.mDepthPyramidDescriptorSet.writeImage(
            1, mResources.mDepthPyramidImage.getRawOtherImageView(i), nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eSampledImage, i
        );
        mResources.mDepthPyramidDescriptorSet.writeImage(
            2, mResources.mDepthPyramidImage.getRawOtherImageView(i), nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage, i
        );
    }
    mResources.mDepthPyramidDescriptorSet.pushWrites();

    vk::Extent3D depthPyramidExtent = mResources.mDepthPyramidImage.getExtent();
    vk::Extent3D depthExtent = vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1};
    mResources.mDepthPyramidPushConstants.mDepthPyramidExtent = glm::uvec2(depthPyramidExtent.width, depthPyramidExtent.height);
    mResources.mDepthPyramidPushConstants.mDepthFullExtent = glm::uvec2(depthExtent.width, depthExtent.height);
    mResources.mDepthPyramidPushConstants.mDepthFullRatio =
        glm::vec2(depthPyramidExtent.width / static_cast<float>(depthExtent.width), depthPyramidExtent.height / static_cast<float>(depthExtent.height));

    // Work pass
    mResources.mWorkDescriptorSet.writeImage(
        0, mResources.mDepthPyramidImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mResources.mWorkDescriptorSet.pushWrites();

    vk::Extent2D drawExtent = sRendererContext.mSwapchain->getWindowExtent();
    mResources.mWorkPushConstants.mDrawExtents = glm::vec2(drawExtent.width, drawExtent.height);
    mResources.mWorkPushConstants.mDepthPyramidExtents = glm::vec2(depthPyramidExtent.width, depthPyramidExtent.height);
}

void SwCull::System::resize() {
    mResources.mDepthPyramidImage.destroy();
    reInitializeOnResize();
}