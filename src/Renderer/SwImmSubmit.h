#pragma once

#include <Renderer/SwRenderer.h>
#include <vulkan/vulkan_raii.hpp>
#include <functional>

class SwImmSubmit {
private:
    static SwRendererContext sRendererContext;
    static vk::raii::CommandPool sCommandPool;
    static vk::raii::CommandBuffer sCommandBuffer;
    static vk::raii::Fence sFence;
    static std::vector<std::function<void(vk::CommandBuffer cmd)>> mCallbacks;

public:
    static void init(SwRendererContext rendererContext);
    static void destroy();

    static void individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function);
    static void queuedSubmit();
};
