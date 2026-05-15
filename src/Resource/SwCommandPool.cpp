#include <Renderer/SwRenderer.h>
#include <Resource/SwCommandPool.h>

SwFactoryContext SwCommandPoolFactory::sRendererContext{};

SwCommandPool::SwCommandPool() : mCommandPool(nullptr) {}

SwCommandPool::SwCommandPool(vk::raii::CommandPool commandPool) : mCommandPool(std::move(commandPool)) {}

void SwCommandPoolFactory::init(SwFactoryContext rendererContext) { sRendererContext = rendererContext; }

SwCommandPool SwCommandPoolFactory::createCommandPool(vk::CommandPoolCreateFlags commandPoolCreateFlags) {
    vk::CommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = commandPoolCreateFlags;
    return SwCommandPool(sRendererContext.mDevice->createCommandPool(commandPoolCreateInfo));
}