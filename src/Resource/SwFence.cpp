#include <Renderer/SwRenderer.h>
#include <Resource/SwFence.h>

SwFactoryContext SwFenceFactory::sRendererContext{};

SwFence::SwFence() : mFence(nullptr) {}

SwFence::SwFence(vk::raii::Fence fence) : mFence(std::move(fence)) {}

void SwFenceFactory::init(SwFactoryContext rendererContext) { sRendererContext = rendererContext; }

SwFence SwFenceFactory::createFence(vk::FenceCreateFlags fenceCreateFlags) {
    vk::FenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = fenceCreateFlags;
    return SwFence(sRendererContext.mDevice->createFence(fenceCreateInfo));
}