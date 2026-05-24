#include <Scene/SwPass.h>

SwPass::SwImageDep::SwImageDep(SwImage* image): mImage(image), mStage(image->getCurrentStage()), mAccess(image->getCurrentAccess()), mLayout(image->getCurrentLayout()) {}

SwPass::SwImageDep::SwImageDep(SwImage* image, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access, vk::ImageLayout layout)
    : mImage(image), mStage(stage), mAccess(access), mLayout(layout) {}

SwPass::SwBufferDep::SwBufferDep(SwBuffer* buffer) : mBuffer(buffer), mStage(buffer->getCurrentStage()), mAccess(buffer->getCurrentAccess()) {}

SwPass::SwBufferDep::SwBufferDep(SwBuffer* buffer, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access) : mBuffer(buffer), mStage(stage), mAccess(access) {}

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
