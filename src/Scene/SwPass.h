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
    PickWork,
    Skybox,
    GeometryOpaque,
    GeometryTransparent,
    WBOITComposite,
    CopyToSwapchain,
    ImGui
};

class SwPass {
public:
    struct SwImageDep {
        SwImage* mImage;
        vk::PipelineStageFlags2 mStage;
        vk::AccessFlags2 mAccess;
        vk::ImageLayout mLayout;
    };
    struct SwBufferDep {
        SwBuffer* mBuffer;
        vk::PipelineStageFlags2 mStage;
        vk::AccessFlags2 mAccess;
    };
    struct SwPassDeps {
        std::vector<SwImageDep> mReadImages;
        std::vector<SwImageDep> mWriteImages;
        std::vector<SwBufferDep> mReadBuffers;
        std::vector<SwBufferDep> mWriteBuffers;

        void clear();
    };

private:
    SwPassType mPassType;
    std::function<void(vk::CommandBuffer)> mCallback;
    bool mMustRun{false};
    bool mPruned{false};

    std::vector<SwImageDep> mReadImages;
    std::vector<SwImageDep> mWriteImages;
    std::vector<SwBufferDep> mReadBuffers;
    std::vector<SwBufferDep> mWriteBuffers;

public:
    SwPass() = default;

    SwPass(
        SwPassType passType, SwPassDeps passDeps, std::function<void(vk::CommandBuffer)> callback, bool mustRun = false
    );

    SwPassType getPassType() const { return mPassType; }
    bool isPruned() const { return mPruned; }
    bool isMustRun() const { return mMustRun; }
    void setPruned(bool pruned) { mPruned = pruned; }

    const std::vector<SwImageDep>& getReadImages() const { return mReadImages; }
    const std::vector<SwImageDep>& getWriteImages() const { return mWriteImages; }
    const std::vector<SwBufferDep>& getReadBuffers() const { return mReadBuffers; }
    const std::vector<SwBufferDep>& getWriteBuffers() const { return mWriteBuffers; }

    void execute(vk::CommandBuffer cmd);
};
