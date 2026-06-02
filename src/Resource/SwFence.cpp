#include <Renderer/SwRenderer.h>
#include <Resource/SwFence.h>


SwFence::SwFence() : mFence(nullptr) {}

SwFence::SwFence(vk::raii::Fence fence) : mFence(std::move(fence)) {}


SwFence SwFenceFactory::createFence(vk::FenceCreateFlags fenceCreateFlags) {
    vk::FenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = fenceCreateFlags;
    return SwFence(SwRenderer::sRendererContext.mDevice->createFence(fenceCreateInfo));
}