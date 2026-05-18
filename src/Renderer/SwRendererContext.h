#pragma once

#include <quill/Logger.h>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>

class SwImmSubmit;
class SwEvents;
class SwCamera;
class SwSwapchain;
class SwDescriptorAllocator;

struct SwContext {
    vk::raii::Device* mDevice;
    quill::Logger* mLogger;

    SwContext() = default;
    SwContext(vk::raii::Device* device, quill::Logger* logger);
};

struct SwFactoryContext : SwContext {
    VmaAllocator mAllocator;
    SwImmSubmit* mImmSubmit;

    SwFactoryContext() = default;
    SwFactoryContext(vk::raii::Device* device, quill::Logger* logger, VmaAllocator allocator, SwImmSubmit* immSubmit);
};

struct SwImmSubmitContext : SwContext {
    VmaAllocator mAllocator;
    vk::raii::Queue* mGraphicsQueue;

    SwImmSubmitContext() = default;
    SwImmSubmitContext(vk::raii::Device* device, quill::Logger* logger, VmaAllocator allocator, vk::raii::Queue* graphicsQueue);
};

struct SwSwapchainContext : SwContext {
    vk::raii::PhysicalDevice* mChosenGPU;
    SwImmSubmit* mImmSubmit;
    SwEvents* mEvents;

    SwSwapchainContext() = default;
    SwSwapchainContext(vk::raii::Device* device, quill::Logger* logger, vk::raii::PhysicalDevice* chosenGPU, SwImmSubmit* immSubmit, SwEvents* events);
};

struct SwGuiContext : SwContext {
    vk::raii::Instance* mInstance;
    vk::raii::PhysicalDevice* mChosenGPU;
    vk::raii::Queue* mGraphicsQueue;
    SwSwapchain* mSwapchain;
    SwEvents* mEvents;
    SwCamera* mCamera;
    SwDescriptorAllocator* mDescriptorAllocator;

    SwGuiContext() = default;
    SwGuiContext(
        vk::raii::Device* device, quill::Logger* logger, vk::raii::Instance* instance, vk::raii::PhysicalDevice* chosenGPU, vk::raii::Queue* graphicsQueue,
        SwSwapchain* swapchain,
        SwEvents* events, SwCamera* mCamera, SwDescriptorAllocator* descriptorAllocator
    );
};

struct SwCameraContext : SwContext {
    SwEvents* mEvents;
    SwSwapchain* mSwapchain;

    SwCameraContext() = default;
    SwCameraContext(vk::raii::Device* device, quill::Logger* logger, SwEvents* events, SwSwapchain* swapchain);
};

struct SwMaterialResourcesContext : SwContext {
    SwDescriptorAllocator* mDescriptorAllocator;

    SwMaterialResourcesContext() = default;
    SwMaterialResourcesContext(vk::raii::Device* device, quill::Logger* logger, SwDescriptorAllocator* descriptorAllocator);
};

struct SwAssetContext : SwContext {
    SwDescriptorAllocator* mDescriptorAllocator;
    SwImmSubmit* mImmSubmit;
    
    SwAssetContext() = default;
    SwAssetContext(vk::raii::Device* device, quill::Logger* logger, SwDescriptorAllocator* descriptorAllocator, SwImmSubmit* immSubmit);
};

