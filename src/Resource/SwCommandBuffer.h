#pragma once

#include <Resource/SwCommandPool.h>

#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwCommandBuffer {
private:
    vk::raii::CommandBuffer mCommandBuffer;
    vk::CommandBufferLevel mLevel;

public:
    SwCommandBuffer();

    SwCommandBuffer(vk::raii::CommandBuffer, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

    inline vk::CommandBuffer getHandle() { return *mCommandBuffer; }

    void reset();

    void begin(vk::CommandBufferUsageFlags commandBufferUsageFlags);

    void end();

    vk::CommandBufferSubmitInfo generateSubmitInfo();
};

class SwCommandBufferFactory {
private:
public:
    static void init();

    static SwCommandBuffer createCommandBuffer(std::string name, SwCommandPool& pool, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
    static SwCommandBuffer createCommandBuffer(std::string name, vk::CommandPool pool, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
};