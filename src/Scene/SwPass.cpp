#include <Misc/SwHelper.h>
#include <Scene/SwPass.h>

SwPass::SwPass(Type passType, SwDependency staticDeps, std::function<void(vk::CommandBuffer)> callback, bool mustRun)
    : mPassType(passType), mStaticDeps(std::move(staticDeps)), mCallback(std::move(callback)), mMustRun(mustRun) {}

void SwPass::execute(vk::CommandBuffer cmd) { mCallback(cmd); }

vk::RenderingInfo SwPass::generateRenderingInfo(
    vk::Extent2D renderExtent, vk::ArrayProxy<vk::RenderingAttachmentInfo> colorAttachments, vk::ArrayProxy<vk::RenderingAttachmentInfo> depthAttachment
) {
    vk::RenderingInfo renderInfo{};
    renderInfo.pNext = nullptr;
    renderInfo.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, renderExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = colorAttachments.size();
    renderInfo.pColorAttachments = colorAttachments.data();
    renderInfo.pDepthAttachment = depthAttachment.data();
    renderInfo.pStencilAttachment = nullptr;
    return renderInfo;
}

void SwPass::setViewportScissors(vk::CommandBuffer cmd, vk::Extent3D imageExtent) {
    vk::Extent2D image2dExtent = SwHelper::extent3dTo2d(imageExtent);
    vk::Viewport viewport = {
        0,
        0,
        static_cast<float>(image2dExtent.width),
        static_cast<float>(image2dExtent.height),
        0.f,
        1.f,
    };
    cmd.setViewport(0, viewport);
    vk::Rect2D scissor = {
        vk::Offset2D{0, 0},
        image2dExtent,
    };
    cmd.setScissor(0, scissor);
}
