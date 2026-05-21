#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwImage.h>

#include <functional>
#include <string>
#include <vector>

class SwPass {
public:
    struct SwImageDep {
        SwAllocatedImage* mImage;
        vk::PipelineStageFlags2 mStage;
        vk::AccessFlags2 mAccess;
        vk::ImageLayout mLayout;
    };
    struct SwBufferDep {
        SwAllocatedBuffer* mBuffer;
        vk::PipelineStageFlags2 mStage;
        vk::AccessFlags2 mAccess;
    };

private:
    std::string mName;
    std::function<void(vk::CommandBuffer)> mCallback;
    bool mMustRun{false};
    bool mPruned{false};

    std::vector<SwImageDep> mReadImages;
    std::vector<SwImageDep> mWriteImages;
    std::vector<SwBufferDep> mReadBuffers;
    std::vector<SwBufferDep> mWriteBuffers;

public:
    SwPass(
        std::string name, std::vector<SwImageDep> readImageDeps, std::vector<SwImageDep> writeImageDeps, std::vector<SwBufferDep> readBufferDeps,
        std::vector<SwBufferDep> writeBufferDeps, std::function<void(vk::CommandBuffer)> callback, bool mustRun = false
    );

    std::string_view getName() const { return mName; }
    bool isPruned() const { return mPruned; }
    bool isMustRun() const { return mMustRun; }
    void setPruned(bool pruned) { mPruned = pruned; }

    const std::vector<SwImageDep>& getReadImages() const { return mReadImages; }
    const std::vector<SwImageDep>& getWriteImages() const { return mWriteImages; }
    const std::vector<SwBufferDep>& getReadBuffers() const { return mReadBuffers; }
    const std::vector<SwBufferDep>& getWriteBuffers() const { return mWriteBuffers; }

    void execute(vk::CommandBuffer cmd);
};
