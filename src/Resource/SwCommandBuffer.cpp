#include <Renderer/SwRenderer.h>
#include <Resource/SwCommandBuffer.h>

SwCommandBuffer::SwCommandBuffer() : mCommandBuffer(nullptr), mLevel(vk::CommandBufferLevel::ePrimary) {}

SwCommandBuffer::SwCommandBuffer(vk::raii::CommandBuffer commandBuffer, vk::CommandBufferLevel level) : mCommandBuffer(std::move(commandBuffer)), mLevel(level) {}

void SwCommandBuffer::reset() { mCommandBuffer.reset(); }

void SwCommandBuffer::begin(vk::CommandBufferUsageFlags commandBufferUsageFlags) {
    vk::CommandBufferInheritanceInfo inheritanceInfo = {};
    vk::CommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.pInheritanceInfo = mLevel == vk::CommandBufferLevel::ePrimary ? nullptr : &inheritanceInfo;
    commandBufferBeginInfo.flags = commandBufferUsageFlags;
    mCommandBuffer.begin(commandBufferBeginInfo);
}

void SwCommandBuffer::end() { mCommandBuffer.end(); }

vk::CommandBufferSubmitInfo SwCommandBuffer::generateSubmitInfo() { return vk::CommandBufferSubmitInfo{*mCommandBuffer, 0}; }

SwCommandBuffer SwCommandBufferFactory::createCommandBuffer(std::string name, SwCommandPool& pool, vk::CommandBufferLevel level) {
    return createCommandBuffer(std::move(name), pool.getHandle(), level);
}

SwCommandBuffer SwCommandBufferFactory::createCommandBuffer(std::string name, vk::CommandPool pool, vk::CommandBufferLevel level) {
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = pool;
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.level = level;

    SwCommandBuffer commandBuffer(std::move(SwRenderer::sRendererContext.mDevice->allocateCommandBuffers(commandBufferAllocateInfo).front()), level);
    SwRenderer::sRendererContext.labelResourceDebug(commandBuffer.getHandle(), name.c_str());
    return commandBuffer;
}