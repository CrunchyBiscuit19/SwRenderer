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

public:
    static void init();

    static SwSemaphore createSemaphore();
};