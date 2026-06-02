#include <Renderer/SwRenderer.h>
#include <Resource/SwSemaphore.h>


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


SwSemaphore SwSemaphoreFactory::createSemaphore() {
    vk::SemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.pNext = nullptr;
    return SwSemaphore(SwRenderer::sRendererContext.mDevice->createSemaphore(semaphoreCreateInfo));
}