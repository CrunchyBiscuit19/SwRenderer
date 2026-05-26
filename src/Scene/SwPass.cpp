#include <Misc/SwHelper.h>
#include <Scene/SwPass.h>

SwDependency::ImageDep::ImageDep(SwImage* image) : mImage(image), mDesc(image->getCurrentStage(), image->getCurrentAccess(), image->getCurrentLayout()) {}

SwDependency::ImageDep::ImageDep(SwImage* image, ImageDepType depType) : mImage(image), mDesc(SwDependency::ImageDepDesc::get(depType)) {}

SwDependency::ImageDep::ImageDep(SwImage* image, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access, vk::ImageLayout layout)
    : mImage(image), mDesc(stage, access, layout) {}

SwDependency::BufferDep::BufferDep(SwBuffer* buffer) : mBuffer(buffer), mDesc(buffer->getCurrentStage(), buffer->getCurrentAccess()) {}

SwDependency::BufferDep::BufferDep(SwBuffer* buffer, BufferDepType depType) : mBuffer(buffer), mDesc(SwDependency::BufferDepDesc::get(depType)) {}

SwDependency::BufferDep::BufferDep(SwBuffer* buffer, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access) : mBuffer(buffer), mDesc(stage, access) {}

constexpr SwDependency::ImageDepDesc SwDependency::ImageDepDesc::get(SwDependency::ImageDepType type) {
    switch (type) {
        case SwDependency::ImageDepType::ColorAttachmentWrite:
            return {vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eColorAttachmentOptimal};

        case SwDependency::ImageDepType::DepthAttachmentWrite:
            return {
                vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                vk::ImageLayout::eDepthAttachmentOptimal
            };

        case SwDependency::ImageDepType::DepthAttachmentReadWrite:
            return {
                vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
                vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                vk::ImageLayout::eDepthAttachmentOptimal
            };
        case SwDependency::ImageDepType::ShaderSampledRead:
            return {vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead, vk::ImageLayout::eShaderReadOnlyOptimal};

        case SwDependency::ImageDepType::TransferSrc:
            return {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead, vk::ImageLayout::eTransferSrcOptimal};

        case SwDependency::ImageDepType::TransferDst:
            return {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal};

        case SwDependency::ImageDepType::ComputeStorageWrite:
            return {vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite, vk::ImageLayout::eGeneral};

        case SwDependency::ImageDepType::ComputeStorageRead:
            return {vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead, vk::ImageLayout::eGeneral};

        case SwDependency::ImageDepType::FragmentStorageWrite:
            return {vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderStorageWrite, vk::ImageLayout::eGeneral};

        case SwDependency::ImageDepType::PresentSrc:
            return {vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlagBits2::eNone, vk::ImageLayout::ePresentSrcKHR};
    }
    std::unreachable();
}

constexpr SwDependency::BufferDepDesc SwDependency::BufferDepDesc::get(SwDependency::BufferDepType type) {
    switch (type) {
        case SwDependency::BufferDepType::VertexShaderStorageRead:
            return {vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead};

        case SwDependency::BufferDepType::IndexRead:
            return {vk::PipelineStageFlagBits2::eIndexInput, vk::AccessFlagBits2::eIndexRead};

        case SwDependency::BufferDepType::IndirectRead:
            return {vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead};

        case SwDependency::BufferDepType::ComputeStorageRead:
            return {vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead};

        case SwDependency::BufferDepType::ComputeStorageWrite:
            return {vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite};
    }
    std::unreachable();
}

void SwDependency::clear() {
    mReadImages.clear();
    mReadBuffers.clear();
    mWriteImages.clear();
    mWriteBuffers.clear();
}

SwPass::SwPass(Type passType, SwDependency passDeps, std::function<void(vk::CommandBuffer)> callback, bool mustRun)
    : mPassType(passType), mDeps(std::move(passDeps)), mCallback(std::move(callback)), mMustRun(mustRun) {}

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
