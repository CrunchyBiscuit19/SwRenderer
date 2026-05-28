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
    static SwRendererContext sRendererContext;

public:
    static void init(SwRendererContext rendererContext);

    static SwCommandBuffer createCommandBuffer(SwCommandPool& pool);
    static SwCommandBuffer createCommandBuffer(vk::CommandPool pool);
};