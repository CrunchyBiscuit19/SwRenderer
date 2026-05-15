#pragma once

#include <Resource/SwCommandBuffer.h>
#include <Resource/SwCommandPool.h>
#include <Resource/SwFence.h>

#include <functional>
#include <vulkan/vulkan_raii.hpp>

struct SwImmSubmitContext;

class SwImmSubmit {
private:
    static SwImmSubmitContext sImmSubmitContext;
    SwCommandPool mCommandPool;
    SwCommandBuffer mCommandBuffer;
    SwFence mFence;
    std::vector<std::function<void(vk::CommandBuffer cmd)>> mCallbacks;

public:
    SwImmSubmit();

    static void init(SwImmSubmitContext immSubmitContext);

    void initialize();

    void individualSubmit(std::function<void(vk::CommandBuffer cmd)>&& function);

    void queuedSubmit();

    void addCallback(std::function<void(vk::CommandBuffer cmd)>&& function);
};
