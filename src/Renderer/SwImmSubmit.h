#pragma once

#include <Resource/SwCommandBuffer.h>
#include <Resource/SwCommandPool.h>
#include <Resource/SwFence.h>

#include <functional>
#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwImmSubmit {
private:
    SwCommandPool mCommandPool;
    SwCommandBuffer mCommandBuffer;
    SwFence mFence;
    std::vector<std::function<void(vk::CommandBuffer cmd)>> mCallbacks;

public:
    SwImmSubmit();

    static void init();

    void initialize();

    void individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function);

    // Runs and clears the accumulated callbacks against the given command buffer without submitting,
    // so per-frame uploads can be folded into the frame's render command buffer.
    void flushInto(vk::CommandBuffer cmd);

    void addCallback(std::function<void(vk::CommandBuffer cmd)>&& function);
};
