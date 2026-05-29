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

    vk::SemaphoreSubmitInfo generateSubmitInfo(vk::PipelineStageFlags2 stageMask) const;
};

class SwSemaphoreFactory {
private:
    static SwRendererContext sRendererContext;

public:
    static void init(SwRendererContext rendererContext);

    static SwSemaphore createSemaphore();
    static SwSemaphore createSignalledSemaphore();
};