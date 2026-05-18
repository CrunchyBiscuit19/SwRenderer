#include <Pass/Cull/SwCullWork.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwShader.h>

SwRendererContext SwCullWorkPass::sRendererContext{};
std::filesystem::path SwCullWorkPass::CULL_WORK_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerCull.comp.spv";

void SwCullWorkPass::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

void SwCullWorkPass::initialize() {
    mWorkDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eCombinedImageSampler, 1}}, vk::ShaderStageFlagBits::eCompute);
    mWorkDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mWorkDescriptorLayout);

    writeDescriptorSet();

    vk::PushConstantRange cullPushConstantRange{};
    cullPushConstantRange.offset = 0;
    cullPushConstantRange.size = sizeof(SwCullWorkPushConstants);
    cullPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout({mWorkDescriptorLayout.getRawLayout()}, cullPushConstantRange);

    SwShader computeShaderModule = SwShaderFactory::createShader(CULL_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);

    mWorkPipelineBundle = SwComputePipelineFactory::createComputePipeline({computeShaderModule.getRawModule(), mWorkPipelineLayout.getRawLayout()});
}

void SwCullWorkPass::writeDescriptorSet() {
    /* mWorkDescriptorSet.writeImage(
        0, depthPyramidImageView, depthPyramidSampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mWorkDescriptorSet.pushWrites();*/ // TODO
}

void SwCullWorkPass::writePushConstants() {
    /*vk::Extent3D depthPyramidExtent = mDepthPyramidImage.extent;
    vk::Extent3D drawExtent = sRendererContext.mSwapchain->;
    mCullPushConstants.renderInstancesCountBuffer = mRenderer->mStats.mRenderInstancesCountBuffer.address.value();
    mCullPushConstants.mainBoundsBuffer = mRenderer->mScene.mMainBoundsBuffer.address.value();
    mCullPushConstants.frustumBuffer = mRenderer->mCamera.mFrustumBuffer.address.value();
    mCullPushConstants.mainNodeTransformsBuffer = mRenderer->mScene.mMainNodeTransformsBuffer.address.value();
    mCullPushConstants.mainInstancesBuffer = mRenderer->mScene.mMainInstancesBuffer.address.value();
    mCullPushConstants.mainVisibleRenderInstancesInstanceIndexBuffer = mRenderer->mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.address.value();
    mCullPushConstants.drawExtents = glm::vec2(drawExtent.width, drawExtent.height);
    mCullPushConstants.depthPyramidExtents = glm::vec2(depthPyramidExtent.width, depthPyramidExtent.height);*/ // TODO
}
