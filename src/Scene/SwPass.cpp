#include <Scene/SwPass.h>

SwPass::SwPass(
    std::string name, std::vector<SwImageDep> readImageDeps, std::vector<SwImageDep> writeImageDeps, std::vector<SwBufferDep> readBufferDeps,
    std::vector<SwBufferDep> writeBufferDeps, std::function<void(vk::CommandBuffer)> callback, bool mustRun
)
    : mName(name),
      mReadImages(std::move(readImageDeps)),
      mWriteImages(std::move(writeImageDeps)),
      mReadBuffers(std::move(readBufferDeps)),
      mWriteBuffers(std::move(writeBufferDeps)),
      mCallback(std::move(callback)),
      mMustRun(mustRun) {}

void SwPass::execute(vk::CommandBuffer cmd) { mCallback(cmd); }