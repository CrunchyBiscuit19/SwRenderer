#pragma once

#include <vulkan/vulkan_raii.hpp>

struct SwFactoryContext;

class SwFence {
private:
    vk::raii::Fence mFence;

public:
    SwFence();

    SwFence(vk::raii::Fence);

    inline vk::Fence getRawFence() { return *mFence; }

    void reset();
};

class SwFenceFactory {
private:
    static SwFactoryContext sRendererContext;

public:
    static void init(SwFactoryContext rendererContext);

    static SwFence createFence(vk::FenceCreateFlags flags);
};