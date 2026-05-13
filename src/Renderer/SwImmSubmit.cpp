#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Resources/SwCommandBuffer.h>
#include <Resources/SwCommandPool.h>
#include <Resources/SwFence.h>

SwImmSubmitContext SwImmSubmit::sImmSubmitContext{};

SwImmSubmit::SwImmSubmit() {}

void SwImmSubmit::init(SwImmSubmitContext rendererContext) { sImmSubmitContext = rendererContext; }

void SwImmSubmit::initialize() {
    mCommandPool = SwCommandPoolFactory::createCommandPool(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    mCommandBuffer = SwCommandBufferFactory::createCommandBuffer(mCommandPool);
    mFence = SwFenceFactory::createFence(vk::FenceCreateFlagBits::eSignaled);
}

void SwImmSubmit::individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function) {
    sImmSubmitContext.mDevice->resetFences(mFence.getRawFence());

    mCommandBuffer.reset();

    mCommandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    function(mCommandBuffer.getRawCommandBuffer());
    mCommandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo = mCommandBuffer.getSubmitInfo();

    vk::SubmitInfo2 submitInfo = {};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    sImmSubmitContext.mGraphicsQueue->submit2(submitInfo, mFence.getRawFence());
    sImmSubmitContext.mDevice->waitForFences(mFence.getRawFence(), true, 1e9);  // DO NOT MOVE THIS TO THE TOP
}

void SwImmSubmit::queuedSubmit() {
    sImmSubmitContext.mDevice->resetFences(mFence.getRawFence());

    mCommandBuffer.reset();

    mCommandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    for (auto& callback : mCallbacks) {
        callback(mCommandBuffer.getRawCommandBuffer());
    }
    mCommandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo = mCommandBuffer.getSubmitInfo();

    vk::SubmitInfo2 submitInfo = {};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    sImmSubmitContext.mGraphicsQueue->submit2(submitInfo, mFence.getRawFence());
    sImmSubmitContext.mDevice->waitForFences(mFence.getRawFence(), true, 1e9);  // DO NOT MOVE THIS TO THE TOP

    mCallbacks.clear();
}

void SwImmSubmit::addCallback(std::function<void(vk::CommandBuffer cmd)>&& function) { mCallbacks.emplace_back(std::move(function)); }
