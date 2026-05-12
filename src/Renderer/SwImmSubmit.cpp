#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>

SwRendererContext SwImmSubmit::sRendererContext{};
vk::raii::CommandPool SwImmSubmit::sCommandPool{nullptr};
vk::raii::CommandBuffer SwImmSubmit::sCommandBuffer{nullptr};
vk::raii::Fence SwImmSubmit::sFence{nullptr};
std::vector<std::function<void(vk::CommandBuffer cmd)>> SwImmSubmit::mCallbacks{};

void SwImmSubmit::init(SwRendererContext rendererContext) {
    sRendererContext = rendererContext;

    vk::CommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.pNext = nullptr;
    commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    sCommandPool = sRendererContext.mDevice->createCommandPool(commandPoolInfo);

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = *sCommandPool;
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    sCommandBuffer = std::move(sRendererContext.mDevice->allocateCommandBuffers(commandBufferAllocateInfo)[0]);

    vk::FenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    sFence = sRendererContext.mDevice->createFence(fenceCreateInfo);
}

void SwImmSubmit::individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function) {
    sRendererContext.mDevice->resetFences(*sFence);

    sCommandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;
    commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    sCommandBuffer.begin(commandBufferBeginInfo);
    function(*sCommandBuffer);
    sCommandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo{};
    commandBufferSubmitInfo.pNext = nullptr;
    commandBufferSubmitInfo.commandBuffer = *sCommandBuffer;
    commandBufferSubmitInfo.deviceMask = 0;

    vk::SubmitInfo2 submitInfo = {};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    sRendererContext.mGraphicsQueue->submit2(submitInfo, *sFence);
    sRendererContext.mDevice->waitForFences(*sFence, true, 1e9);  // DO NOT MOVE THIS TO THE TOP
}

void SwImmSubmit::queuedSubmit() {
    sRendererContext.mDevice->resetFences(*sFence);

    sCommandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;
    commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    sCommandBuffer.begin(commandBufferBeginInfo);
    for (auto& callback : mCallbacks) {
        callback(*sCommandBuffer);
    }
    sCommandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo{};
    commandBufferSubmitInfo.pNext = nullptr;
    commandBufferSubmitInfo.commandBuffer = *sCommandBuffer;
    commandBufferSubmitInfo.deviceMask = 0;

    vk::SubmitInfo2 submitInfo = {};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    sRendererContext.mGraphicsQueue->submit2(submitInfo, *sFence);
    sRendererContext.mDevice->waitForFences(*sFence, true, 1e9);  // DO NOT MOVE THIS TO THE TOP

    mCallbacks.clear();
}

void SwImmSubmit::destroy() {
    sCommandBuffer.clear();
    sCommandPool.clear();
    sFence.clear();
}