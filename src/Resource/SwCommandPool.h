#pragma once

#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwCommandPool {
private:
    vk::raii::CommandPool mCommandPool;

public:
    SwCommandPool();

    SwCommandPool(vk::raii::CommandPool);

    inline vk::CommandPool getRawCommandPool() { return *mCommandPool; }
};

class SwCommandPoolFactory {
private:
    static SwRendererContext sRendererContext;

public:
    static void init(SwRendererContext rendererContext);

    static SwCommandPool createCommandPool(vk::CommandPoolCreateFlags commandPoolCreateFlags);
};