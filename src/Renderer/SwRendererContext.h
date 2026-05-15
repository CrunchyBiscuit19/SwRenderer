#pragma once

#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwDescriptor.h>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan_raii.hpp>

struct SwFactoryContext {
    vk::raii::Device* mDevice;
    VmaAllocator mAllocator;
    SwImmSubmit* mImmSubmit;

    SwFactoryContext() = default;

    SwFactoryContext(vk::raii::Device* device, VmaAllocator allocator, SwImmSubmit* immSubmit)
        : mDevice(device), mAllocator(allocator), mImmSubmit(immSubmit) {};
};

struct SwImmSubmitContext {
    vk::raii::Device* mDevice;
    VmaAllocator mAllocator;
    vk::raii::Queue* mGraphicsQueue;

    SwImmSubmitContext() = default;

    SwImmSubmitContext(vk::raii::Device* device, VmaAllocator allocator, vk::raii::Queue* graphicsQueue)
        : mDevice(device), mAllocator(allocator), mGraphicsQueue(graphicsQueue) {};
};

struct SwSwapchainContext {
    vk::raii::Device* mDevice;
    vk::raii::PhysicalDevice* mChosenGPU;
    SwImmSubmit* mImmSubmit;
    SwEvents* mEvents;

    SwSwapchainContext() = default;

    SwSwapchainContext(vk::raii::Device* device, vk::raii::PhysicalDevice* chosenGPU, SwImmSubmit* immSubmit, SwEvents* events)
        : mDevice(device), mChosenGPU(chosenGPU), mImmSubmit(immSubmit), mEvents(events) {};
};

struct SwGuiContext {
    vk::raii::Instance* mInstance;
    vk::raii::Device* mDevice;
    vk::raii::PhysicalDevice* mChosenGPU;
    vk::raii::Queue* mGraphicsQueue;
    SwSwapchain* mSwapchain;
    SwEvents* mEvents;
    SwDescriptorAllocator* mDescriptorAllocator;

    SwGuiContext() = default;

    SwGuiContext(
        vk::raii::Instance* instance, vk::raii::Device* device, vk::raii::PhysicalDevice* chosenGPU, vk::raii::Queue* graphicsQueue, SwSwapchain* swapchain,
        SwEvents* events, SwDescriptorAllocator* descriptorAllocator
    )
        : mInstance(instance),
          mDevice(device),
          mChosenGPU(chosenGPU),
          mGraphicsQueue(graphicsQueue),
          mSwapchain(swapchain),
          mEvents(events),
          mDescriptorAllocator(descriptorAllocator) {};
};

struct SwCameraContext {
    vk::raii::Device* mDevice;
    SwEvents* mEvents;
    SwSwapchain* mSwapchain;

    SwCameraContext() = default;

    SwCameraContext(vk::raii::Device* device, SwEvents* events, SwSwapchain* swapchain) : mDevice(device), mEvents(events), mSwapchain(swapchain) {};
};