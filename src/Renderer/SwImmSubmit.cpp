#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Resource/SwCommandBuffer.h>
#include <Resource/SwCommandPool.h>
#include <Resource/SwFence.h>


SwImmSubmit::SwImmSubmit() {}


void SwImmSubmit::initialize() {
    mCommandPool = SwCommandPoolFactory::createCommandPool("ImmSubmitCommandPool", vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    mCommandBuffer = SwCommandBufferFactory::createCommandBuffer("ImmSubmitCommandBuffer", mCommandPool);
    mFence = SwFenceFactory::createFence("ImmSubmitFence", vk::FenceCreateFlagBits::eSignaled);
}

void SwImmSubmit::individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function) {
    SwRenderer::sRendererContext.mDevice->resetFences(mFence.getRawFence());

    mCommandBuffer.reset();

    mCommandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    function(mCommandBuffer.getRawCommandBuffer());
    mCommandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo = mCommandBuffer.generateSubmitInfo();

    vk::SubmitInfo2 submitInfo = {};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    SwRenderer::sRendererContext.mGraphicsQueue->submit2(submitInfo, mFence.getRawFence());
    vk::Result result = SwRenderer::sRendererContext.mDevice->waitForFences(mFence.getRawFence(), true, 1e9);  // DO NOT MOVE THIS TO THE TOP
}

void SwImmSubmit::queuedSubmit() {
    SwRenderer::sRendererContext.mDevice->resetFences(mFence.getRawFence());

    mCommandBuffer.reset();

    mCommandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    for (auto& callback : mCallbacks) {
        callback(mCommandBuffer.getRawCommandBuffer());
    }
    mCommandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo = mCommandBuffer.generateSubmitInfo();

    vk::SubmitInfo2 submitInfo = {};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    SwRenderer::sRendererContext.mGraphicsQueue->submit2(submitInfo, mFence.getRawFence());
    vk::Result result = SwRenderer::sRendererContext.mDevice->waitForFences(mFence.getRawFence(), true, 1e9);  // DO NOT MOVE THIS TO THE TOP

    mCallbacks.clear();
}

void SwImmSubmit::addCallback(std::function<void(vk::CommandBuffer cmd)>&& function) { mCallbacks.emplace_back(std::move(function)); }
