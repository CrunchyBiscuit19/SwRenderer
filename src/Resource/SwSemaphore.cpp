#include <Renderer/SwRenderer.h>
#include <Resource/SwSemaphore.h>

SwRendererContext SwSemaphoreFactory::sRendererContext{};

SwSemaphore::SwSemaphore() : mSemaphore(nullptr) {}

SwSemaphore::SwSemaphore(vk::raii::Semaphore semaphore) : mSemaphore(std::move(semaphore)) {}

void SwSemaphoreFactory::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

SwSemaphore SwSemaphoreFactory::createSemaphore() {
    vk::SemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.pNext = nullptr;
    return SwSemaphore(sRendererContext.mDevice->createSemaphore(semaphoreCreateInfo));
}