#include <Renderer/SwRenderer.h>
#include <Resource/SwCommandPool.h>


SwCommandPool::SwCommandPool() : mCommandPool(nullptr) {}

SwCommandPool::SwCommandPool(vk::raii::CommandPool commandPool) : mCommandPool(std::move(commandPool)) {}


SwCommandPool SwCommandPoolFactory::createCommandPool(vk::CommandPoolCreateFlags commandPoolCreateFlags) {
    vk::CommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = commandPoolCreateFlags;
    return SwCommandPool(SwRenderer::sRendererContext.mDevice->createCommandPool(commandPoolCreateInfo));
}