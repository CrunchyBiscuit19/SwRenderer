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
#include <Data/SwMaterial.h>
#include <Data/SwAsset.h>
#include <SDL_vulkan.h>
#include <Vkbootstrap.h>
#include <fmt/core.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
) {
    auto* renderer = static_cast<SwRenderer*>(pUserData);

    std::string severity;
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            severity = "ERROR";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            severity = "WARNING";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            severity = "INFO";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            severity = "VERBOSE";
            break;
        default:
            severity = "UNKNOWN";
            break;
    }

    std::string queueLabels;
    for (std::uint32_t i = 0; i < pCallbackData->queueLabelCount; ++i) {
        queueLabels += fmt::format("LabelName = <{}>\n", pCallbackData->pQueueLabels[i].pLabelName);
    }

    std::string cmdBufLabels;
    for (std::uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; ++i) {
        cmdBufLabels += fmt::format("LabelName = <{}>\n", pCallbackData->pCmdBufLabels[i].pLabelName);
    }

    std::string resources;
    for (std::uint32_t i = 0; i < pCallbackData->objectCount; ++i) {
        const auto& object = pCallbackData->pObjects[i];

        resources += fmt::format(
            "Resource {} -> [ ResourceType = {}, ResourceHandle = {}]\n", i, vk::to_string(static_cast<vk::ObjectType>(object.objectType)), object.objectHandle
        );

        if (object.pObjectName) {
            resources += fmt::format("ResourceName   = <{}>\n", object.pObjectName);
        }
    }

    std::string message = fmt::format(
        "\n{} <{}> Frame {}\n{}\nQueue Labels: {}\nCommandBuffer Labels: {}\n{}",
        severity,
        pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "",
        renderer->getFrameNumber(),
        pCallbackData->pMessage,
        queueLabels,
        cmdBufLabels,
        resources
    );

    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            LOG_ERROR(renderer->getLogger(), "{}", message);
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            LOG_WARNING(renderer->getLogger(), "{}", message);
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            LOG_TRACE_L3(renderer->getLogger(), "{}", message);
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        default:
            break;
    }

    return vk::False;
}

SwRenderer::SwRenderer()
    : mContext(), mDebugMessenger(nullptr), mInstance(nullptr), mDevice(nullptr), mChosenGPU(nullptr), mComputeQueue(nullptr), mGraphicsQueue(nullptr) {
    quill::Backend::start();
    auto fileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(
        fmt::format("{}Run.log", LOGS_PATH).c_str(),
        []() {
            quill::FileSinkConfig cfg;
            cfg.set_open_mode('w');
            cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
            return cfg;
        }(),
        quill::FileEventNotifier{}
    );
    auto latestFileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(
        fmt::format("{}Latest.log", LOGS_PATH).c_str(),
        []() {
            quill::FileSinkConfig cfg;
            cfg.set_open_mode('w');
            return cfg;
        }(),
        quill::FileEventNotifier{}
    );
    auto consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink1");
    quill::PatternFormatterOptions options{};
    options.format_pattern = "[%(short_source_location) | %(time) | %(log_level)] \n %(message)";
    options.timestamp_pattern = "%H:%M:%S.%Qms";
    options.add_metadata_to_multi_line_logs = false;
    if (LOG_LOCATION == LogLocation::File) {
        mLogger = quill::Frontend::create_or_get_logger("LOGGER", {std::move(fileSink), std::move(latestFileSink)}, options);
    } else if (LOG_LOCATION == LogLocation::Console) {
        mLogger = quill::Frontend::create_or_get_logger("LOGGER", std::move(consoleSink), options);
    } else if (LOG_LOCATION == LogLocation::Both) {
        mLogger = quill::Frontend::create_or_get_logger("LOGGER", {std::move(fileSink), std::move(latestFileSink), std::move(consoleSink)}, options);
    }
    mLogger->set_log_level(quill::LogLevel::Info);

    vk::Extent2D windowExtent(1700, 900);
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow(
        "SwRenderer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        static_cast<std::uint32_t>(windowExtent.width),
        static_cast<std::uint32_t>(windowExtent.height),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (FULLSCREEN_ON_STARTUP ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0)
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
        .set_debug_callback(debugMessageFunc)
        .set_debug_callback_user_data_pointer(this)
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
    SDL_Vulkan_CreateSurface(window, *mInstance, &tempSurface);
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

    mFactoryContext = SwFactoryContext(&mDevice, mLogger, mAllocator.mAllocator, &mImmSubmit);
    mImmSubmitContext = SwImmSubmitContext(&mDevice, mLogger, mAllocator.mAllocator, &mGraphicsQueue);
    mSwapchainContext = SwSwapchainContext(&mDevice, mLogger, &mChosenGPU, &mImmSubmit, &mEvents);
    mGuiContext = SwGuiContext(&mDevice, mLogger, &mInstance, &mChosenGPU, &mGraphicsQueue, &mSwapchain, &mEvents, &mCamera, &mDescriptorAllocator);
    mCameraContext = SwCameraContext(&mDevice, mLogger, &mEvents, &mSwapchain);
    mMaterialResourcesContext = SwMaterialResourcesContext(&mDevice, mLogger, &mDescriptorAllocator);
    mAssetContext = SwAssetContext(&mDevice, mLogger, &mDescriptorAllocator, &mImmSubmit);

    SwSemaphoreFactory::init(mFactoryContext);
    SwFenceFactory::init(mFactoryContext);
    SwCommandPoolFactory::init(mFactoryContext);
    SwCommandBufferFactory::init(mFactoryContext);

    SwImmSubmit::init(mImmSubmitContext);
    mImmSubmit.initialize();

    SwShaderFactory::init(mFactoryContext);
    SwSamplerFactory::init(mFactoryContext);
    SwDescriptorAllocator::init(mFactoryContext);
    SwPipelineFactory::init(mFactoryContext);
    SwBufferFactory::init(mFactoryContext);
    SwImageFactory::init(mFactoryContext);
    
    SwMesh::init();
    SwBounds::init();
    SwNode::init();
    SwMaterialConstants::init();

    SwSwapchain::init(mSwapchainContext);
    mSwapchain.initialize(window, std::move(surface), windowExtent, FULLSCREEN_ON_STARTUP);
    mStats.initialize();

    SwGui::init(mGuiContext);
    mGui.initialize();
    
    SwCamera::init(mCameraContext);
    mCamera.initialize();

    SwMaterialResources::init(mMaterialResourcesContext);
    SwMaterial::init();
    SwAsset::init(mAssetContext);

}

SwRenderer::~SwRenderer() {
    SwMesh::cleanup();
    SwBounds::cleanup();
    SwNode::cleanup();
    SwMaterialConstants::cleanup();
    SwImageFactory::cleanup();
    SwMaterialResources::cleanup();
    SwMaterial::cleanup();
    SwAsset::cleanup();
    SDL_Quit();
}