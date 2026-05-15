#pragma once

#include <vulkan/vulkan_raii.hpp>

struct SwFactoryContext;

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
    static SwFactoryContext sRendererContext;

public:
    static void init(SwFactoryContext rendererContext);

    static SwCommandPool createCommandPool(vk::CommandPoolCreateFlags commandPoolCreateFlags);
};