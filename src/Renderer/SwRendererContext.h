#pragma once

#include <quill/Logger.h>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>

class SwImmSubmit;
class SwEvents;
class SwCamera;
class SwSwapchain;
class SwDescriptorAllocator;

struct SwRendererContext {
    vk::raii::Instance* mInstance;
    vk::raii::PhysicalDevice* mChosenGPU;
    vk::raii::Device* mDevice;
    VmaAllocator mAllocator;
    vk::raii::Queue* mGraphicsQueue;
    vk::raii::Queue* mComputeQueue;
    SwDescriptorAllocator* mDescriptorAllocator;
    SwSwapchain* mSwapchain;
    SwImmSubmit* mImmSubmit;
    SwEvents* mEvents;
    SwCamera* mCamera;
    quill::Logger* mLogger;

    SwRendererContext() = default;
    SwRendererContext(
        vk::raii::Instance* instance, vk::raii::PhysicalDevice* chosenGPU, vk::raii::Device* device, VmaAllocator allocator, vk::raii::Queue* graphicsQueue,
        vk::raii::Queue* computeQueue, SwDescriptorAllocator* descriptorAllocator, SwSwapchain* swapchain, SwImmSubmit* immSubmit, SwEvents* events,
        SwCamera* camera, quill::Logger* logger
    );
};