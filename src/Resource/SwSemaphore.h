#pragma once

#include <vulkan/vulkan_raii.hpp>

struct SwFactoryContext;

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
    static SwFactoryContext sRendererContext;

public:
    static void init(SwFactoryContext rendererContext);

    static SwSemaphore createSemaphore();
};