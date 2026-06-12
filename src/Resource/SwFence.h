#pragma once

#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

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

public:
    static void init();

    static SwFence createFence(std::string name, vk::FenceCreateFlags flags);
};