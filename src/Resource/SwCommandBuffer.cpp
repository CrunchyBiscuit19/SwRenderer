#include <Renderer/SwRenderer.h>
#include <Resource/SwCommandBuffer.h>


SwCommandBuffer::SwCommandBuffer(): mCommandBuffer(nullptr) {}

SwCommandBuffer::SwCommandBuffer(vk::raii::CommandBuffer commandBuffer) : mCommandBuffer(std::move(commandBuffer)) {}

void SwCommandBuffer::reset() { mCommandBuffer.reset(); }

void SwCommandBuffer::begin(vk::CommandBufferUsageFlags commandBufferUsageFlags) {
    vk::CommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;
    commandBufferBeginInfo.flags = commandBufferUsageFlags;
    mCommandBuffer.begin(commandBufferBeginInfo);
}

void SwCommandBuffer::end() { mCommandBuffer.end(); }

vk::CommandBufferSubmitInfo SwCommandBuffer::generateSubmitInfo() {
    return vk::CommandBufferSubmitInfo{
        *mCommandBuffer, 0
    };
}


SwCommandBuffer SwCommandBufferFactory::createCommandBuffer(std::string name, SwCommandPool& pool) {
    return createCommandBuffer(std::move(name), pool.getRawCommandPool());
}

SwCommandBuffer SwCommandBufferFactory::createCommandBuffer(std::string name, vk::CommandPool pool) {
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = pool;
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;

    SwCommandBuffer commandBuffer(std::move(SwRenderer::sRendererContext.mDevice->allocateCommandBuffers(commandBufferAllocateInfo).front()));
    SwRenderer::sRendererContext.labelResourceDebug(commandBuffer.getRawCommandBuffer(), name.c_str());
    return commandBuffer;
}