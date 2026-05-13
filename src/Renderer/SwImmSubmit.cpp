#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>

SwRendererContext SwImmSubmit::sRendererContext{};

SwImmSubmit::SwImmSubmit() : mCommandPool(nullptr), mCommandBuffer(nullptr), mFence(nullptr) {}

void SwImmSubmit::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

void SwImmSubmit::initialize() {
    vk::CommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.pNext = nullptr;
    commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    mCommandPool = sRendererContext.mDevice->createCommandPool(commandPoolInfo);

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = *mCommandPool;
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    mCommandBuffer = std::move(sRendererContext.mDevice->allocateCommandBuffers(commandBufferAllocateInfo)[0]);

    vk::FenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    mFence = sRendererContext.mDevice->createFence(fenceCreateInfo);
}

void SwImmSubmit::individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function) {
    sRendererContext.mDevice->resetFences(*mFence);

    mCommandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;
    commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    mCommandBuffer.begin(commandBufferBeginInfo);
    function(*mCommandBuffer);
    mCommandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo{};
    commandBufferSubmitInfo.pNext = nullptr;
    commandBufferSubmitInfo.commandBuffer = *mCommandBuffer;
    commandBufferSubmitInfo.deviceMask = 0;

    vk::SubmitInfo2 submitInfo = {};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    sRendererContext.mGraphicsQueue->submit2(submitInfo, *mFence);
    sRendererContext.mDevice->waitForFences(*mFence, true, 1e9);  // DO NOT MOVE THIS TO THE TOP
}

void SwImmSubmit::queuedSubmit() {
    sRendererContext.mDevice->resetFences(*mFence);

    mCommandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;
    commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    mCommandBuffer.begin(commandBufferBeginInfo);
    for (auto& callback : mCallbacks) {
        callback(*mCommandBuffer);
    }
    mCommandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo{};
    commandBufferSubmitInfo.pNext = nullptr;
    commandBufferSubmitInfo.commandBuffer = *mCommandBuffer;
    commandBufferSubmitInfo.deviceMask = 0;

    vk::SubmitInfo2 submitInfo = {};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    sRendererContext.mGraphicsQueue->submit2(submitInfo, *mFence);
    sRendererContext.mDevice->waitForFences(*mFence, true, 1e9);  // DO NOT MOVE THIS TO THE TOP

    mCallbacks.clear();
}