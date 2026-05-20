#include <Misc/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwScene.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>

std::filesystem::path SwScene::CULL_RESET_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerReset.comp.spv";
std::filesystem::path SwScene::CULL_WORK_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerCull.comp.spv";
std::filesystem::path SwScene::CULL_COMPACT_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerCompact.comp.spv";
std::filesystem::path SwScene::CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerDepthPyramid.comp.spv";

SwRendererContext SwScene::sRendererContext{};

void SwScene::initializeCullResources() {
    // Push pass
    vk::PushConstantRange resetPushConstantRange = SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(SwCull::ResetPC));
    mCullResources.mResetPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, resetPushConstantRange);

    SwShader resetShader = SwShaderFactory::createShader(CULL_RESET_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mResetPipelinePipeline =
        SwComputePipelineFactory::createComputePipeline({resetShader.getRawModule(), mCullResources.mResetPipelineLayout.getRawLayout()});

    // Depth pyramid pass
    mCullResources.mDepthPyramidDescriptorLayout = sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage},
         {1, vk::DescriptorType::eSampledImage, CULL_MAX_DEPTH_PYRAMID_LEVELS},
         {2, vk::DescriptorType::eStorageImage, CULL_MAX_DEPTH_PYRAMID_LEVELS}},
        vk::ShaderStageFlagBits::eCompute
    );

    mCullResources.mDepthPyramidDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mCullResources.mDepthPyramidDescriptorLayout);

    vk::PushConstantRange depthPyramidPushConstantRange =
        SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(SwCull::DepthPyramidPC));
    mCullResources.mDepthPyramidPipelineLayout =
        SwPipelineFactory::createPipelineLayout({mCullResources.mDepthPyramidDescriptorLayout.getRawLayout()}, depthPyramidPushConstantRange);

    vk::ShaderModule depthPyramidShader =
        SwShaderFactory::createShader(CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute).getRawModule();
    mCullResources.mDepthPyramidPipelinePipeline =
        SwComputePipelineFactory::createComputePipeline({depthPyramidShader, mCullResources.mDepthPyramidPipelineLayout.getRawLayout()});

    // Work pass
    mCullResources.mWorkDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eCombinedImageSampler, 1}}, vk::ShaderStageFlagBits::eCompute);

    mCullResources.mWorkDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mCullResources.mWorkDescriptorLayout);

    vk::PushConstantRange workPushConstantRange = SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(SwCull::WorkPC));
    mCullResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout({mCullResources.mWorkDescriptorLayout.getRawLayout()}, workPushConstantRange);

    SwShader workShader = SwShaderFactory::createShader(CULL_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mWorkPipelinePipeline =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mCullResources.mWorkPipelineLayout.getRawLayout()});

    // Compact pass
    vk::PushConstantRange compactPushConstantRange =
        SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(SwCull::CompactPC));
    mCullResources.mCompactPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, compactPushConstantRange);

    SwShader compactShader = SwShaderFactory::createShader(CULL_COMPACT_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mCompactPipelinePipeline =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mCullResources.mCompactPipelineLayout.getRawLayout()});

    // Everything that needs to be re-built on resize
    reInitializableCullResources();
}

void SwScene::reInitializableCullResources() {
    // Depth pyramid pass
    std::uint32_t depthPyramidWidth = swHelper::previousPow2(sRendererContext.mSwapchain->getDepthImage().getExtent().width);
    std::uint32_t depthPyramidHeight = swHelper::previousPow2(sRendererContext.mSwapchain->getDepthImage().getExtent().height);
    mCullResources.mDepthPyramidExtent = vk::Extent3D{
        depthPyramidWidth,
        depthPyramidHeight,
        1,
    };
    mCullResources.mDepthPyramidLevels = swHelper::calculateMipMapLevels(mCullResources.mDepthPyramidExtent);
    mCullResources.mDepthPyramidImage = SwImageFactory::createColorImage2D(
        nullptr, mCullResources.mDepthPyramidExtent, vk::Format::eR32Sfloat, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
    );

    /*mDepthPyramidMipViews.clear();
    mDepthPyramidMipViews.reserve(1 + mDepthPyramidLevels);
    for (std::uint32_t i = 0; i < mDepthPyramidLevels; i++) {
        vk::ImageViewCreateInfo levelInfo = vkhelper::imageViewCreateInfo(mDepthPyramidImage.format, mDepthPyramidImage.image, vk::ImageAspectFlagBits::eColor);
        levelInfo.subresourceRange.levelCount = 1;
        levelInfo.subresourceRange.baseMipLevel = i;
        mDepthPyramidMipViews.emplace_back(std::move(mRenderer->mCore.mDevice.createImageView(levelInfo)));
        mRenderer->mCore.labelResourceDebug(mDepthPyramidMipViews.back(), fmt::format("CullerDepthPyramidMipView{}", i).c_str());
    }*/ // TODO after overhauling image abstraction

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mCullResources.mDepthPyramidImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
        );
    });

    // Work pass
    mCullResources.mWorkDescriptorSet.writeImage(
        0, mCullResources.mDepthPyramidImage.getRawImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mCullResources.mWorkDescriptorSet.pushWrites();

    // vk::Extent3D drawExtent = mRenderer->mInfrastructure.mDrawImage.extent;
    // mCullResources.mWorkPushConstants.mRenderInstancesCountBuffer = mRenderer->mStats.mRenderInstancesCountBuffer.address.value();
    // mCullResources.mWorkPushConstants.mSceneBoundsBuffer = mRenderer->mScene.mMainBoundsBuffer.address.value();
    // mCullResources.mWorkPushConstants.mFrustumBuffer = mRenderer->mCamera.mFrustumBuffer.address.value();
    // mCullResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mRenderer->mScene.mMainNodeTransformsBuffer.address.value();
    // mCullResources.mWorkPushConstants.mSceneInstancesBuffer = mRenderer->mScene.mMainInstancesBuffer.address.value();
    // mCullResources.mWorkPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
    // mRenderer->mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.address.value(); mCullResources.mWorkPushConstants.mDrawExtents =
    // glm::vec2(sRendererContext.mSwapchain->ge, drawExtent.height); mCullResources.mWorkPushConstants.mDepthPyramidExtents =
    //     glm::vec2(mCullResources.mDepthPyramidImage.getExtent().width, mCullResources.mDepthPyramidImage.getExtent().height);
}
