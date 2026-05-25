#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwImage.h>

#include <functional>
#include <string>
#include <vector>

enum class SwPassType {
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

class SwPass {
public:
    struct ImageDep {
        SwImage* mImage;
        vk::PipelineStageFlags2 mStage;
        vk::AccessFlags2 mAccess;
        vk::ImageLayout mLayout;

        ImageDep(SwImage* image);
        ImageDep(SwImage* image, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access, vk::ImageLayout layout);
    };
    struct BufferDep {
        SwBuffer* mBuffer;
        vk::PipelineStageFlags2 mStage;
        vk::AccessFlags2 mAccess;

        BufferDep(SwBuffer* buffer);
        BufferDep(SwBuffer* buffer, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access);
    };
    struct PassDeps {
        std::vector<ImageDep> mReadImages;
        std::vector<ImageDep> mWriteImages;
        std::vector<BufferDep> mReadBuffers;
        std::vector<BufferDep> mWriteBuffers;

        void clear();
    };

private:
    SwPassType mPassType;
    std::function<void(vk::CommandBuffer)> mCallback;
    bool mMustRun{false};
    bool mPruned{false};

    std::vector<ImageDep> mReadImages;
    std::vector<ImageDep> mWriteImages;
    std::vector<BufferDep> mReadBuffers;
    std::vector<BufferDep> mWriteBuffers;

public:
    SwPass() = default;

    SwPass(
        SwPassType passType, PassDeps passDeps, std::function<void(vk::CommandBuffer)> callback, bool mustRun = false
    );

    SwPassType getPassType() const { return mPassType; }
    bool isPruned() const { return mPruned; }
    bool isMustRun() const { return mMustRun; }
    void setPruned(bool pruned) { mPruned = pruned; }

    const std::vector<ImageDep>& getReadImages() const { return mReadImages; }
    const std::vector<ImageDep>& getWriteImages() const { return mWriteImages; }
    const std::vector<BufferDep>& getReadBuffers() const { return mReadBuffers; }
    const std::vector<BufferDep>& getWriteBuffers() const { return mWriteBuffers; }

    void execute(vk::CommandBuffer cmd);

    static vk::RenderingInfo generateRenderingInfo(
        vk::Extent2D renderExtent, vk::ArrayProxy<vk::RenderingAttachmentInfo> colorAttachments, vk::ArrayProxy<vk::RenderingAttachmentInfo> depthAttachment
    );
    static void setViewportScissors(vk::CommandBuffer cmd, vk::Extent3D imageExtent);
};
