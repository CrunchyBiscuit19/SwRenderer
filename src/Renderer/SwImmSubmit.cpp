#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Resource/SwCommandBuffer.h>
#include <Resource/SwCommandPool.h>
#include <Resource/SwFence.h>

SwImmSubmit::SwImmSubmit() {}

void SwImmSubmit::initialize() {
    mCommandPool = SwCommandPoolFactory::createCommandPool(
        "ImmSubmitCommandPool", vk::CommandPoolCreateFlagBits::eResetCommandBuffer, SwRenderer::sRendererContext.mGraphicsQueueFamily
    );
    mCommandBuffer = SwCommandBufferFactory::createCommandBuffer("ImmSubmitCommandBuffer", mCommandPool);
    mFence = SwFenceFactory::createFence("ImmSubmitFence", vk::FenceCreateFlagBits::eSignaled);
}

void SwImmSubmit::individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function) {
    SwRenderer::sRendererContext.mDevice->resetFences(mFence.getHandle());

    mCommandBuffer.reset();

    mCommandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    function(mCommandBuffer.getHandle());
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

    SwRenderer::sRendererContext.mGraphicsQueue->submit2(submitInfo, mFence.getHandle());
    vk::Result result = SwRenderer::sRendererContext.mDevice->waitForFences(mFence.getHandle(), true, 1e9);  // DO NOT MOVE THIS TO THE TOP
}

void SwImmSubmit::flushInto(SwQueueType queueType, vk::CommandBuffer cmd) {
    for (auto& flushIntoCallback : mCallbacks[queueType]) {
        flushIntoCallback(cmd);
    }
    mCallbacks[queueType].clear();
}

void SwImmSubmit::addCallback(SwQueueType queueType, std::function<void(vk::CommandBuffer cmd)>&& function) {
    mCallbacks[queueType].emplace_back(std::move(function));
}