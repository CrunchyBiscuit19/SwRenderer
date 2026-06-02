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

    void queuedSubmit();

    void addCallback(std::function<void(vk::CommandBuffer cmd)>&& function);
};
