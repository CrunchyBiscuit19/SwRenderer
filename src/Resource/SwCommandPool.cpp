#include <Renderer/SwRenderer.h>
#include <Resource/SwCommandPool.h>


SwCommandPool::SwCommandPool() : mCommandPool(nullptr) {}

SwCommandPool::SwCommandPool(vk::raii::CommandPool commandPool) : mCommandPool(std::move(commandPool)) {}


SwCommandPool SwCommandPoolFactory::createCommandPool(std::string name, vk::CommandPoolCreateFlags commandPoolCreateFlags) {
    vk::CommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = commandPoolCreateFlags;
    SwCommandPool commandPool(SwRenderer::sRendererContext.mDevice->createCommandPool(commandPoolCreateInfo));
    SwRenderer::sRendererContext.labelResourceDebug(commandPool.getHandle(), name.c_str());
    return commandPool;
}