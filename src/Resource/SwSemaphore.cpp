#include <Renderer/SwRenderer.h>
#include <Resource/SwSemaphore.h>

SwFactoryContext SwSemaphoreFactory::sRendererContext{};

SwSemaphore::SwSemaphore() : mSemaphore(nullptr) {}

SwSemaphore::SwSemaphore(vk::raii::Semaphore semaphore) : mSemaphore(std::move(semaphore)) {}

void SwSemaphoreFactory::init(SwFactoryContext rendererContext) { sRendererContext = rendererContext; }

SwSemaphore SwSemaphoreFactory::createSemaphore() {
    vk::SemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.pNext = nullptr;
    return SwSemaphore(sRendererContext.mDevice->createSemaphore(semaphoreCreateInfo));
}