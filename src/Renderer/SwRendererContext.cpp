#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwLogger.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwDescriptor.h>

#include <algorithm>

SwRendererContext::SwRendererContext(
    vk::raii::Instance* instance, vk::raii::PhysicalDevice* chosenGPU, vk::raii::Device* device, VmaAllocator allocator, vk::raii::Queue* graphicsQueue,
    vk::raii::Queue* computeQueue, vk::raii::Queue* transferQueue, std::uint32_t graphicsQueueFamily, std::uint32_t computeQueueFamily,
    std::uint32_t transferQueueFamily, SwDescriptorAllocator* descriptorAllocator, SwSwapchain* swapchain, SwImmSubmit* immSubmit, SwStagingRing* stagingRing,
    SwEvents* events, SwScene* scene, SwStats* stats, SwLogger* logger
)
    : mInstance(instance),
      mChosenGPU(chosenGPU),
      mDevice(device),
      mAllocator(allocator),
      mGraphicsQueue(graphicsQueue),
      mComputeQueue(computeQueue),
      mTransferQueue(transferQueue),
      mGraphicsQueueFamily(graphicsQueueFamily),
      mComputeQueueFamily(computeQueueFamily),
      mTransferQueueFamily(transferQueueFamily),
      mDescriptorAllocator(descriptorAllocator),
      mSwapchain(swapchain),
      mImmSubmit(immSubmit),
      mStagingRing(stagingRing),
      mEvents(events),
      mScene(scene),
      mStats(stats),
      mLogger(logger) {}
