#include <Renderer/SwRendererContext.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwDescriptor.h>

#include <vulkan/vulkan_raii.hpp>

SwFactoryContext::SwFactoryContext(vk::raii::Device* device, VmaAllocator allocator, SwImmSubmit* immSubmit)
    : mDevice(device), mAllocator(allocator), mImmSubmit(immSubmit) {}

SwImmSubmitContext::SwImmSubmitContext(vk::raii::Device* device, VmaAllocator allocator, vk::raii::Queue* graphicsQueue)
    : mDevice(device), mAllocator(allocator), mGraphicsQueue(graphicsQueue) {}

SwSwapchainContext::SwSwapchainContext(vk::raii::Device* device, vk::raii::PhysicalDevice* chosenGPU, SwImmSubmit* immSubmit, SwEvents* events)
    : mDevice(device), mChosenGPU(chosenGPU), mImmSubmit(immSubmit), mEvents(events) {}

SwGuiContext::SwGuiContext(
    vk::raii::Instance* instance,
    vk::raii::Device* device,
    vk::raii::PhysicalDevice* chosenGPU,
    vk::raii::Queue* graphicsQueue,
    SwSwapchain* swapchain,
    SwEvents* events,
    SwCamera* camera,
    SwDescriptorAllocator* descriptorAllocator
)
    : mInstance(instance),
      mDevice(device),
      mChosenGPU(chosenGPU),
      mGraphicsQueue(graphicsQueue),
      mSwapchain(swapchain),
      mEvents(events),
      mCamera(camera),
      mDescriptorAllocator(descriptorAllocator) {}

SwCameraContext::SwCameraContext(vk::raii::Device* device, SwEvents* events, SwSwapchain* swapchain)
    : mDevice(device), mEvents(events), mSwapchain(swapchain) {}

SwMaterialResourcesContext::SwMaterialResourcesContext(vk::raii::Device* device, SwDescriptorAllocator* descriptorAllocator): mDevice(device), mDescriptorAllocator(descriptorAllocator) {}
