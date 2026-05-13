#pragma once

#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwEvents.h>
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
    SwEvents* mEvents;

    SwSwapchainContext() = default;

    SwSwapchainContext(
        vk::raii::Device* device, vk::raii::PhysicalDevice* chosenGPU, SwEvents* events)
        : mDevice(device), mChosenGPU(chosenGPU), mEvents(events) {};
};