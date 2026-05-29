#pragma once

struct SwImage;
struct SwBuffer;

struct SwDependency {
public:
    enum class ImageDepType {
        ColorAttachmentWrite,
        DepthAttachmentWrite,
        DepthAttachmentReadWrite,
        ShaderSampledRead,
        TransferSrc,
        TransferDst,
        ComputeStorageWrite,
        ComputeStorageRead,
        ComputeStorageReadWrite,
        FragmentStorageWrite,
        PresentSrc
    };
    struct ImageDepDesc {
        vk::PipelineStageFlags2 mStage;
        vk::AccessFlags2 mAccess;
        vk::ImageLayout mLayout;

        static constexpr ImageDepDesc get(ImageDepType type);
    };
    struct ImageDep {
        SwImage* mImage;
        ImageDepDesc mDesc;

        ImageDep(SwImage* image);
        ImageDep(SwImage* image, ImageDepType depType);
        ImageDep(SwImage* image, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access, vk::ImageLayout layout);
    };

    enum class BufferDepType { VertexShaderStorageRead, IndexRead, IndirectRead, ComputeStorageRead, ComputeStorageWrite, TransferWrite, HostWrite };
    struct BufferDepDesc {
        vk::PipelineStageFlags2 mStage;
        vk::AccessFlags2 mAccess;

        static constexpr BufferDepDesc get(BufferDepType type);
    };
    struct BufferDep {
        SwBuffer* mBuffer;
        BufferDepDesc mDesc;

        BufferDep(SwBuffer* buffer);
        BufferDep(SwBuffer* buffer, BufferDepType depType);
        BufferDep(SwBuffer* buffer, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access);
    };

    std::vector<ImageDep> mReadImages;
    std::vector<ImageDep> mWriteImages;
    std::vector<BufferDep> mReadBuffers;
    std::vector<BufferDep> mWriteBuffers;

    void clear();
};