#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>

SwFrame::SwFrame() : mCommandPool(nullptr), mCommandBuffer(nullptr), mRenderFence(nullptr), mAvailableSemaphore(nullptr) {}

void SwFrame::initialize() {

}