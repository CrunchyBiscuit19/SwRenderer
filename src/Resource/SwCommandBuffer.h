#pragma once

#include <Resource/SwCommandPool.h>

#include <vulkan/vulkan_raii.hpp>

struct SwFactoryContext;

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

    vk::CommandBufferSubmitInfo getSubmitInfo();
};

class SwCommandBufferFactory {
private:
    static SwFactoryContext sRendererContext;

public:
    static void init(SwFactoryContext rendererContext);

    static SwCommandBuffer createCommandBuffer(SwCommandPool& pool);
    static SwCommandBuffer createCommandBuffer(vk::CommandPool pool);
};