#include <Scene/SwPass.h>

SwPass::SwPass(SwPassType passType, SwPassDeps passDeps, std::function<void(vk::CommandBuffer)> callback, bool mustRun)
    : mPassType(passType),
      mReadImages(std::move(passDeps.mReadImages)),
      mWriteImages(std::move(passDeps.mWriteImages)),
      mReadBuffers(std::move(passDeps.mReadBuffers)),
      mWriteBuffers(std::move(passDeps.mWriteBuffers)),
      mCallback(std::move(callback)),
      mMustRun(mustRun) {}

void SwPass::execute(vk::CommandBuffer cmd) { mCallback(cmd); }

void SwPass::SwPassDeps::clear() {
    mReadImages.clear();
    mReadBuffers.clear();
    mWriteImages.clear();
    mWriteBuffers.clear();
}
