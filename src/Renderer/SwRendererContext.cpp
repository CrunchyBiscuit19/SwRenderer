#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwLogger.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwDescriptor.h>

#include <vulkan/vulkan_raii.hpp>

SwRendererContext::SwRendererContext(
    vk::raii::Instance* instance, vk::raii::PhysicalDevice* chosenGPU, vk::raii::Device* device, VmaAllocator allocator, vk::raii::Queue* graphicsQueue,
    vk::raii::Queue* computeQueue,
    SwDescriptorAllocator* descriptorAllocator, SwSwapchain* swapchain, SwImmSubmit* immSubmit, SwEvents* events, SwScene* scene, SwStats* stats, SwLogger* logger
)
    : mInstance(instance),
      mChosenGPU(chosenGPU),
      mDevice(device),
      mAllocator(allocator),
      mGraphicsQueue(graphicsQueue),
      mComputeQueue(computeQueue),
      mDescriptorAllocator(descriptorAllocator),
      mSwapchain(swapchain),
      mImmSubmit(immSubmit),
      mEvents(events),
      mScene(scene),
      mStats(stats),
      mLogger(logger) {}
