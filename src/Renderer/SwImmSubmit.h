#pragma once

#include <Renderer/SwQueueType.h>
#include <Resource/SwCommandBuffer.h>
#include <Resource/SwCommandPool.h>
#include <Resource/SwFence.h>

#include <functional>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwImmSubmit {
private:
    SwCommandPool mCommandPool;
    SwCommandBuffer mCommandBuffer;
    SwFence mFence;
    std::unordered_map<SwQueueType, std::vector<std::function<void(vk::CommandBuffer cmd)>>> mCallbacks;

public:
    SwImmSubmit();

    static void init();

    void initialize();

    void individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function);

    void flushInto(SwQueueType queueType, vk::CommandBuffer cmd);
    void addCallback(SwQueueType queueType, std::function<void(vk::CommandBuffer cmd)>&& function);
};
