#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwImage.h>

#include <functional>
#include <string>
#include <vector>

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

class SwPass {
public:
    enum class Type {
        ClearImages,
        CullReset,
        CullDepthPyramid,
        CullWork,
        CullCompact,
        PickDraw,
        PickReadback,
        PickWork,
        Skybox,
        GeometryOpaque,
        GeometryTransparent,
        WBOITComposite,
        CopyToSwapchain,
        Gui
    };

private:
    Type mPassType;
    std::function<void(vk::CommandBuffer)> mCallback;
    bool mMustRun{false};
    bool mPruned{false};

    SwDependency mDeps;

public:
    SwPass() = default;

    SwPass(Type passType, SwDependency passDeps, std::function<void(vk::CommandBuffer)> callback, bool mustRun = false);

    Type getPassType() const { return mPassType; }
    bool isPruned() const { return mPruned; }
    bool isMustRun() const { return mMustRun; }
    void setPruned(bool pruned) { mPruned = pruned; }
    const SwDependency& getDeps() const { return mDeps; }

    void execute(vk::CommandBuffer cmd);

    static vk::RenderingInfo generateRenderingInfo(
        vk::Extent2D renderExtent, vk::ArrayProxy<vk::RenderingAttachmentInfo> colorAttachments, vk::ArrayProxy<vk::RenderingAttachmentInfo> depthAttachment
    );
    static void setViewportScissors(vk::CommandBuffer cmd, vk::Extent3D imageExtent);
};
