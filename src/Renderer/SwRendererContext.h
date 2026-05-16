#pragma once

#include <vk_mem_alloc.h>

class vk::raii::Device;
class vk::raii::PhysicalDevice;
class vk::raii::Queue;
class vk::raii::Instance;
class SwImmSubmit;
class SwEvents;
class SwCamera;
class SwSwapchain;
class SwDescriptorAllocator;

struct SwFactoryContext {
    vk::raii::Device* mDevice;
    VmaAllocator mAllocator;
    SwImmSubmit* mImmSubmit;

    SwFactoryContext() = default;
    SwFactoryContext(vk::raii::Device* device, VmaAllocator allocator, SwImmSubmit* immSubmit);
};

struct SwImmSubmitContext {
    vk::raii::Device* mDevice;
    VmaAllocator mAllocator;
    vk::raii::Queue* mGraphicsQueue;

    SwImmSubmitContext() = default;
    SwImmSubmitContext(vk::raii::Device* device, VmaAllocator allocator, vk::raii::Queue* graphicsQueue);
};

struct SwSwapchainContext {
    vk::raii::Device* mDevice;
    vk::raii::PhysicalDevice* mChosenGPU;
    SwImmSubmit* mImmSubmit;
    SwEvents* mEvents;

    SwSwapchainContext() = default;
    SwSwapchainContext(vk::raii::Device* device, vk::raii::PhysicalDevice* chosenGPU, SwImmSubmit* immSubmit, SwEvents* events);
};

struct SwGuiContext {
    vk::raii::Instance* mInstance;
    vk::raii::Device* mDevice;
    vk::raii::PhysicalDevice* mChosenGPU;
    vk::raii::Queue* mGraphicsQueue;
    SwSwapchain* mSwapchain;
    SwEvents* mEvents;
    SwCamera* mCamera;
    SwDescriptorAllocator* mDescriptorAllocator;

    SwGuiContext() = default;
    SwGuiContext(
        vk::raii::Instance* instance, vk::raii::Device* device, vk::raii::PhysicalDevice* chosenGPU, vk::raii::Queue* graphicsQueue, SwSwapchain* swapchain,
        SwEvents* events, SwCamera* mCamera, SwDescriptorAllocator* descriptorAllocator
    );
};

struct SwCameraContext {
    vk::raii::Device* mDevice;
    SwEvents* mEvents;
    SwSwapchain* mSwapchain;

    SwCameraContext() = default;
    SwCameraContext(vk::raii::Device* device, SwEvents* events, SwSwapchain* swapchain);
};

struct SwMaterialResourcesContext {
    vk::raii::Device* mDevice;
    SwDescriptorAllocator* mDescriptorAllocator;

    SwMaterialResourcesContext() = default;
    SwMaterialResourcesContext(vk::raii::Device* device, SwDescriptorAllocator* descriptorAllocator);
};