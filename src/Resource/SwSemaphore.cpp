#include <Renderer/SwRenderer.h>
#include <Resource/SwSemaphore.h>

SwRendererContext SwSemaphoreFactory::sRendererContext{};

SwSemaphore::SwSemaphore() : mSemaphore(nullptr) {}

SwSemaphore::SwSemaphore(vk::raii::Semaphore semaphore) : mSemaphore(std::move(semaphore)) {}

vk::SemaphoreSubmitInfo SwSemaphore::generateSubmitInfo(vk::PipelineStageFlags2 stageMask) const { 
    vk::SemaphoreSubmitInfo semaphoreSubmitInfo; 
    semaphoreSubmitInfo.pNext = nullptr;
    semaphoreSubmitInfo.semaphore = *mSemaphore;
    semaphoreSubmitInfo.stageMask = stageMask;
    semaphoreSubmitInfo.deviceIndex = 0;
    semaphoreSubmitInfo.value = 1;
    return semaphoreSubmitInfo;
}

void SwSemaphoreFactory::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

SwSemaphore SwSemaphoreFactory::createSemaphore() {
    vk::SemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.pNext = nullptr;
    return SwSemaphore(sRendererContext.mDevice->createSemaphore(semaphoreCreateInfo));
}

SwSemaphore SwSemaphoreFactory::createSignalledSemaphore() {
    vk::SemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.pNext = nullptr;
    vk::raii::Semaphore semaphore = sRendererContext.mDevice->createSemaphore(semaphoreCreateInfo);

    vk::SemaphoreSubmitInfo signalInfo = {};
    signalInfo.semaphore = *semaphore;
    signalInfo.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
    signalInfo.deviceIndex = 0;
    signalInfo.value = 0;

    vk::SubmitInfo2 submitInfo = {};
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalInfo;
    sRendererContext.mGraphicsQueue->submit2(submitInfo, nullptr);

    return SwSemaphore(std::move(semaphore));
}