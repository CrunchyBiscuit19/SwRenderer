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

public:
    static void init();

    static SwCommandPool createCommandPool(std::string name, vk::CommandPoolCreateFlags commandPoolCreateFlags);
};