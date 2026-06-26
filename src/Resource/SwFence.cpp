#include <Renderer/SwRenderer.h>
#include <Resource/SwFence.h>


SwFence::SwFence() : mFence(nullptr) {}

SwFence::SwFence(vk::raii::Fence fence) : mFence(std::move(fence)) {}


SwFence SwFenceFactory::createFence(std::string name, vk::FenceCreateFlags fenceCreateFlags) {
    vk::FenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = fenceCreateFlags;
    SwFence fence(SwRenderer::sRendererContext.mDevice->createFence(fenceCreateInfo));
    SwRenderer::sRendererContext.labelResourceDebug(fence.getHandle(), name.c_str());
    return fence;
}