#include <Scene/SwDependency.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwImage.h>

SwDependency::ImageDep::ImageDep(SwImage* image) : mImage(image), mDesc(image->getCurrentStage(), image->getCurrentAccess(), image->getCurrentLayout()) {}

SwDependency::ImageDep::ImageDep(SwImage* image, ImageDepType depType) : mImage(image), mDesc(SwDependency::ImageDepDesc::get(depType)) {}

SwDependency::ImageDep::ImageDep(SwImage* image, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access, vk::ImageLayout layout)
    : mImage(image), mDesc(stage, access, layout) {}

const SwDependency::ImageDepDesc SwDependency::ImageDepDesc::get(SwDependency::ImageDepType type) {
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
        case SwDependency::ImageDepType::ComputeShaderSampledRead:
            return {vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead, vk::ImageLayout::eShaderReadOnlyOptimal};
        case SwDependency::ImageDepType::FragmentShaderSampledRead:
            return {vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead, vk::ImageLayout::eShaderReadOnlyOptimal};
        case SwDependency::ImageDepType::TransferSrc:
            return {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead, vk::ImageLayout::eTransferSrcOptimal};
        case SwDependency::ImageDepType::TransferDst:
            return {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal};
        case SwDependency::ImageDepType::ComputeStorageWrite:
            return {vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite, vk::ImageLayout::eGeneral};
        case SwDependency::ImageDepType::ComputeStorageRead:
            return {vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead, vk::ImageLayout::eGeneral};
        case SwDependency::ImageDepType::ComputeStorageReadWrite:
            return {
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
                vk::ImageLayout::eGeneral
            };
        case SwDependency::ImageDepType::FragmentStorageWrite:
            return {vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderStorageWrite, vk::ImageLayout::eGeneral};
        case SwDependency::ImageDepType::PresentSrc:
            return {vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eNone, vk::ImageLayout::ePresentSrcKHR};
    }
    std::unreachable();
}

SwDependency::BufferDep::BufferDep(SwBuffer* buffer) : mBuffer(buffer), mDesc(buffer->getCurrentStage(), buffer->getCurrentAccess()) {}

SwDependency::BufferDep::BufferDep(SwBuffer* buffer, BufferDepType depType) : mBuffer(buffer), mDesc(SwDependency::BufferDepDesc::get(depType)) {}

SwDependency::BufferDep::BufferDep(SwBuffer* buffer, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access) : mBuffer(buffer), mDesc(stage, access) {}

const SwDependency::BufferDepDesc SwDependency::BufferDepDesc::get(SwDependency::BufferDepType type) {
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
        case SwDependency::BufferDepType::TransferWrite:
            return {vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite};
        case SwDependency::BufferDepType::HostWrite:
            return {vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostWrite};
    }
    std::unreachable();
}

void SwDependency::clear() {
    mReadImages.clear();
    mReadBuffers.clear();
    mWriteImages.clear();
    mWriteBuffers.clear();
}
