#pragma once

#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwSemaphore {
private:
    vk::raii::Semaphore mSemaphore;

public:
    SwSemaphore();

    SwSemaphore(vk::raii::Semaphore);

    inline vk::Semaphore getRawSemaphore() { return *mSemaphore; }
};

class SwSemaphoreFactory {
private:
    static SwRendererContext sRendererContext;

public:
    static void init(SwRendererContext rendererContext);

    static SwSemaphore createSemaphore();
};