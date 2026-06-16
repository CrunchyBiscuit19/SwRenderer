#include <Data/SwAsset.h>
#include <Data/SwMaterial.h>
#include <Renderer/SwRenderer.h>
#include <Resource/SwBuffer.h>
#include <Resource/SwCommandBuffer.h>
#include <Resource/SwCommandPool.h>
#include <Resource/SwDescriptor.h>
#include <Resource/SwFence.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwSampler.h>
#include <Resource/SwSemaphore.h>
#include <Resource/SwShader.h>
#include <System/SwIBL.h>
#include <SDL3/SDL_vulkan.h>
#include <Vkbootstrap.h>
#include <fmt/core.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

SwRendererContext SwRenderer::sRendererContext{};

SwRenderer::SwRenderer()
    : mContext(),
      mDebugMessenger(nullptr),
      mInstance(nullptr),
      mDevice(nullptr),
      mChosenGPU(nullptr),
      mComputeQueue(nullptr),
      mGraphicsQueue(nullptr),
      mDescriptorAllocator(
          {
              {vk::DescriptorType::eCombinedImageSampler, 1},
              {vk::DescriptorType::eSampledImage, 1},
              {vk::DescriptorType::eStorageImage, 1},
              {vk::DescriptorType::eUniformBuffer, 1},
              {vk::DescriptorType::eSampler, 1},
          },
          MAX_DESCRIPTOR_SETS
      ) {
    vk::Extent2D windowExtent(1700, 900);
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow(
        "SwRenderer",
        static_cast<int>(windowExtent.width),
        static_cast<int>(windowExtent.height),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (FULLSCREEN_ON_STARTUP ? SDL_WINDOW_FULLSCREEN : 0)
    );
    float aspectRatio = static_cast<float>(windowExtent.width) / static_cast<float>(windowExtent.height);

    mContext = vk::raii::Context();

    vkb::InstanceBuilder vkbInstBuilder;
    vkbInstBuilder.set_app_name("SwRenderer")
        .set_debug_messenger_severity(
            static_cast<VkDebugUtilsMessageSeverityFlagsEXT>(
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
            )
        )
        .set_debug_messenger_type(
            static_cast<VkDebugUtilsMessageTypeFlagsEXT>(
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
            )
        )
        .set_debug_callback(SwLogger::debugMessageFunc)
        .set_debug_callback_user_data_pointer(&mLogger)
        .require_api_version(VK_MAJOR_VERSION, VK_MINOR_VERSION, VK_PATCH_VERSION);
    if (VALIDATION_MODE >= ValidationMode::Basic) {
        vkbInstBuilder.request_validation_layers(true);
    }
    if (VALIDATION_MODE >= ValidationMode::Strict) {
        vkbInstBuilder.add_validation_feature_enable(static_cast<VkValidationFeatureEnableEXT>(vk::ValidationFeatureEnableEXT::eDebugPrintf))
            .add_validation_feature_enable(static_cast<VkValidationFeatureEnableEXT>(vk::ValidationFeatureEnableEXT::eGpuAssisted))
            .add_validation_feature_enable(static_cast<VkValidationFeatureEnableEXT>(vk::ValidationFeatureEnableEXT::eGpuAssistedReserveBindingSlot))
            .add_validation_feature_enable(static_cast<VkValidationFeatureEnableEXT>(vk::ValidationFeatureEnableEXT::eSynchronizationValidation))
            .add_validation_feature_enable(static_cast<VkValidationFeatureEnableEXT>(vk::ValidationFeatureEnableEXT::eBestPractices));
    }

    const vkb::Instance vkbInst = vkbInstBuilder.build().value();
    mInstance = vk::raii::Instance(mContext, vkbInst.instance);
    vk::raii::DebugUtilsMessengerEXT debugMessenger(mInstance, vkbInst.debug_messenger);
    mDebugMessenger = std::move(debugMessenger);

    VkSurfaceKHR tempSurface = nullptr;
    SDL_Vulkan_CreateSurface(window, *mInstance, nullptr, &tempSurface);
    vk::raii::SurfaceKHR surface(mInstance, tempSurface);

    vk::PhysicalDeviceVulkan14Features features14{};
    vk::PhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;
    vk::PhysicalDeviceVulkan12Features features12{};
    features12.separateDepthStencilLayouts = true;
    features12.scalarBlockLayout = true;
    features12.samplerFilterMinmax = true;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    features12.drawIndirectCount = true;
    features12.runtimeDescriptorArray = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.timelineSemaphore = true;
    features12.vulkanMemoryModel = true;
    features12.vulkanMemoryModelDeviceScope = true;
    features12.storageBuffer8BitAccess = true;
    vk::PhysicalDeviceVulkan11Features features11{};
    features11.shaderDrawParameters = true;
    vk::PhysicalDeviceFeatures features10{};
    features10.independentBlend = true;
    features10.shaderStorageImageMultisample = true;
    features10.multiDrawIndirect = true;
    features10.samplerAnisotropy = true;
    features10.sampleRateShading = true;
    features10.drawIndirectFirstInstance = true;
    features10.fragmentStoresAndAtomics = true;
    features10.vertexPipelineStoresAndAtomics = true;
    features10.shaderInt64 = true;

    vk::PhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT unusedAttachmentsFeatures{};
    unusedAttachmentsFeatures.dynamicRenderingUnusedAttachments = vk::True;
    vk::PhysicalDeviceComputeShaderDerivativesFeaturesKHR computeShaderDerivativesFeatures{};
    computeShaderDerivativesFeatures.computeDerivativeGroupLinear = vk::True;
    computeShaderDerivativesFeatures.computeDerivativeGroupQuads = vk::True;

    vkb::PhysicalDeviceSelector selector{vkbInst};
    vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(VK_MAJOR_VERSION, VK_MINOR_VERSION)
                                             .add_required_extension(vk::KHRSwapchainMutableFormatExtensionName)
                                             .add_required_extension(vk::EXTDynamicRenderingUnusedAttachmentsExtensionName)
                                             .add_required_extension(vk::KHRComputeShaderDerivativesExtensionName)
                                             .add_required_extension(vk::KHRDepthStencilResolveExtensionName)
                                             .add_required_extension(vk::EXTSamplerFilterMinmaxExtensionName)
                                             .add_required_extension_features(unusedAttachmentsFeatures)
                                             .add_required_extension_features(computeShaderDerivativesFeatures)
                                             .set_required_features_13(features13)
                                             .set_required_features_12(features12)
                                             .set_required_features_11(features11)
                                             .set_required_features(features10)
                                             .set_surface(*surface)
                                             .select()
                                             .value();
    vkb::DeviceBuilder deviceBuilder{physicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();
    vk::raii::PhysicalDevice chosenGPU(mInstance, vkbDevice.physical_device);
    vk::raii::Device device(chosenGPU, vkbDevice.device);
    mChosenGPU = std::move(chosenGPU);
    mDevice = std::move(device);
    mChosenGPUProperties = mChosenGPU.getProperties();

    vk::raii::Queue computeQueue(mDevice, vkbDevice.get_queue(vkb::QueueType::compute).value());
    vk::raii::Queue graphicsQueue(mDevice, vkbDevice.get_queue(vkb::QueueType::graphics).value());
    mComputeQueue = std::move(computeQueue);
    mGraphicsQueue = std::move(graphicsQueue);
    mComputeQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
    mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = *mChosenGPU;
    allocatorInfo.device = *mDevice;
    allocatorInfo.instance = *mInstance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &mAllocator.mAllocator);

    mEvents.addEventCallback([this](SDL_Event& e) -> void {
        if (e.type == SDL_EVENT_QUIT) {
            mScene.markAllAssetsDelete();
            mSwapchain.setProgramEndFrameNumber(mSwapchain.getFrameNumber() + SwSwapchain::NUM_FRAME_OVERLAP + 1);
        }
        if (e.type == SDL_EVENT_WINDOW_MINIMIZED) mStopRendering = true;
        if (e.type == SDL_EVENT_WINDOW_RESTORED) mStopRendering = false;
        ImGui_ImplSDL3_ProcessEvent(&e);
    });

    sRendererContext = SwRendererContext(
        &mInstance,
        &mChosenGPU,
        &mDevice,
        mAllocator.mAllocator,
        &mGraphicsQueue,
        &mComputeQueue,
        &mDescriptorAllocator,
        &mSwapchain,
        &mImmSubmit,
        &mEvents,
        &mScene,
        &mStats,
        &mLogger
    );

    mImmSubmit.initialize();

    SwSamplerFactory::init();
    SwImageFactory::init();

    SwMesh::init();
    SwBounds::init();
    SwNode::init();
    SwLight::init();
    SwMaterialConstants::init();

    mSwapchain.initialize(window, std::move(surface), windowExtent, FULLSCREEN_ON_STARTUP);
    mLogger.setFrameNumber(mSwapchain.getFrameNumberPtr());
    mStats.initialize();

    SwMaterialResources::init();
    SwIBL::System::init();  
    SwMaterial::init();

    mScene.initialize();
}

void SwRenderer::run() {
    SDL_Event e;

    while (true) {
        auto start = std::chrono::system_clock::now();

        if (mSwapchain.getProgramEndFrameNumber().has_value() && (mSwapchain.getFrameNumber() < mSwapchain.getProgramEndFrameNumber().value())) {
            mDevice.waitIdle();
            break;
        }

        while (SDL_PollEvent(&e) != 0) {
            mEvents.executeEventCallbacks(e);
        }

        if (mStopRendering) {
            // Do not draw if minimized, throttle to avoid endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        SDL_SetWindowRelativeMouseMode(mSwapchain.getWindowPtr(), mScene.getCamera().getRelativeMode());
        if (mSwapchain.getResizeRequested()) {
            mDevice.waitIdle();

            mSwapchain.resize();
            mScene.resize();
            
            mSwapchain.setResizeRequested(false);
            mDevice.waitIdle();
        }

        mScene.perFrameUpdate();
        mStats.perFrameReset();
        mSwapchain.getCurrentFrame().update();

        mScene.draw();

        mSwapchain.incrementFrameNumber();

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        mStats.mFrameTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
    }
}

SwRenderer::~SwRenderer() {
    SwMesh::cleanup();
    SwBounds::cleanup();
    SwNode::cleanup();
    SwLight::cleanup();
    SwMaterialConstants::cleanup();
    SwIBL::System::cleanup();
    SwMaterialResources::cleanup();
    SwImageFactory::cleanup();
    SwMaterial::cleanup();
    SwAsset::cleanup();
    SwSamplerFactory::cleanup();
    SDL_Quit();
}