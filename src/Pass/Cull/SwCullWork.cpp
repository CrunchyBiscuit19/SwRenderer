#include <Pass/Cull/SwCullWork.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwShader.h>


void SwCullWorkPass::writeDescriptorSet() {
    /* mWorkDescriptorSet.writeImage(
        0, depthPyramidImageView, depthPyramidSampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mWorkDescriptorSet.pushWrites();*/ // TODO pass implementation
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
    mCullPushConstants.depthPyramidExtents = glm::vec2(depthPyramidExtent.width, depthPyramidExtent.height);*/ // TODO scene and pass implementation
}

