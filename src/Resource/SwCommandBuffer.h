#pragma once

#include <Resource/SwCommandPool.h>

#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwCommandBuffer {
private:
    vk::raii::CommandBuffer mCommandBuffer;

public:
    SwCommandBuffer();

    SwCommandBuffer(vk::raii::CommandBuffer);

    inline vk::CommandBuffer getRawCommandBuffer() { return *mCommandBuffer; }

    void reset();

    void begin(vk::CommandBufferUsageFlags commandBufferUsageFlags);

    void end();

    vk::CommandBufferSubmitInfo generateSubmitInfo();
};

class SwCommandBufferFactory {
private:

public:
    static void init();

    static SwCommandBuffer createCommandBuffer(std::string name, SwCommandPool& pool);
    static SwCommandBuffer createCommandBuffer(std::string name, vk::CommandPool pool);
};