#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <functional>

struct SwRendererContext;

class SwImmSubmit {
private:
    static SwRendererContext sRendererContext;
    vk::raii::CommandPool mCommandPool;
    vk::raii::CommandBuffer mCommandBuffer;
    vk::raii::Fence mFence;
    std::vector<std::function<void(vk::CommandBuffer cmd)>> mCallbacks;

public:
    SwImmSubmit();

    static void init(SwRendererContext rendererContext);

    void initialize();

    void individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function);
    
    void queuedSubmit();
};
