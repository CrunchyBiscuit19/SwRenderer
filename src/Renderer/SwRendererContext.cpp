#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwDescriptor.h>

#include <vulkan/vulkan_raii.hpp>

SwContext::SwContext(vk::raii::Device* device, quill::Logger* logger) : mDevice(device), mLogger(logger) {}

SwFactoryContext::SwFactoryContext(vk::raii::Device* device, quill::Logger* logger, VmaAllocator allocator, SwImmSubmit* immSubmit)
    : SwContext(device, logger), mAllocator(allocator), mImmSubmit(immSubmit) {}

SwImmSubmitContext::SwImmSubmitContext(vk::raii::Device* device, quill::Logger* logger, VmaAllocator allocator, vk::raii::Queue* graphicsQueue)
    : SwContext(device, logger), mAllocator(allocator), mGraphicsQueue(graphicsQueue) {}

SwSwapchainContext::SwSwapchainContext(
    vk::raii::Device* device, quill::Logger* logger, vk::raii::PhysicalDevice* chosenGPU, SwImmSubmit* immSubmit, SwEvents* events
)
    : SwContext(device, logger), mChosenGPU(chosenGPU), mImmSubmit(immSubmit), mEvents(events) {}

SwGuiContext::SwGuiContext(
    vk::raii::Device* device, quill::Logger* logger, vk::raii::Instance* instance, vk::raii::PhysicalDevice* chosenGPU, vk::raii::Queue* graphicsQueue,
    SwSwapchain* swapchain, SwEvents* events, SwCamera* camera, SwDescriptorAllocator* descriptorAllocator
)
    : SwContext(device, logger),
      mInstance(instance),
      mChosenGPU(chosenGPU),
      mGraphicsQueue(graphicsQueue),
      mSwapchain(swapchain),
      mEvents(events),
      mCamera(camera),
      mDescriptorAllocator(descriptorAllocator) {}

SwCameraContext::SwCameraContext(vk::raii::Device* device, quill::Logger* logger, SwEvents* events, SwSwapchain* swapchain)
    : SwContext(device, logger), mEvents(events), mSwapchain(swapchain) {}

SwMaterialResourcesContext::SwMaterialResourcesContext(vk::raii::Device* device, quill::Logger* logger, SwDescriptorAllocator* descriptorAllocator)
    : SwContext(device, logger), mDescriptorAllocator(descriptorAllocator) {}

SwAssetContext::SwAssetContext(vk::raii::Device* device, quill::Logger* logger, SwDescriptorAllocator* descriptorAllocator, SwImmSubmit* immSubmit)
    : SwContext(device, logger), mDescriptorAllocator(descriptorAllocator), mImmSubmit(immSubmit) {}
