#include <Renderer/SwLogger.h>
#include <fmt/core.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

#include <cctype>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// VK name translation helpers
// ---------------------------------------------------------------------------

// Converts one underscore-delimited segment to eCamelCase form.
// Groups of alpha chars are title-cased; groups of digits are kept verbatim.
// Examples: "GENERAL" -> "General", "UINT32" -> "Uint32",
//           "R8G8B8A8" -> "R8G8B8A8", "BC7" -> "Bc7", "D32" -> "D32".
static std::string convertSegment(std::string_view seg) {
    std::string out;
    out.reserve(seg.size());
    std::size_t i = 0;
    while (i < seg.size()) {
        if (std::isalpha(static_cast<unsigned char>(seg[i]))) {
            std::size_t j = i;
            while (j < seg.size() && std::isalpha(static_cast<unsigned char>(seg[j]))) ++j;
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(seg[i])));
            for (std::size_t k = i + 1; k < j; ++k)
                out += static_cast<char>(std::tolower(static_cast<unsigned char>(seg[k])));
            i = j;
        } else if (std::isdigit(static_cast<unsigned char>(seg[i]))) {
            std::size_t j = i;
            while (j < seg.size() && std::isdigit(static_cast<unsigned char>(seg[j]))) ++j;
            out += seg.substr(i, j - i);
            i = j;
        } else {
            out += seg[i++];
        }
    }
    return out;
}

// Converts "VK_SOME_PREFIX_VALUE_BIT_KHR" to "vk::SomeType::eValue" given a
// (prefix, cppType) pair. Returns empty string if prefix doesn't match.
static std::string tryConvertVkName(std::string_view token) {
    struct Entry {
        std::string_view prefix;
        std::string_view cppType;
    };

    // Sorted by prefix length descending so longer prefixes are tried first,
    // preventing VK_FORMAT_ from matching before VK_FORMAT_FEATURE_ etc.
    static constexpr Entry kPrefixes[] = {
        // length 38
        {"VK_BUILD_ACCELERATION_STRUCTURE_MODE_",    "vk::BuildAccelerationStructureModeKHR"},
        // length 33
        {"VK_DEBUG_UTILS_MESSAGE_SEVERITY_",         "vk::DebugUtilsMessageSeverityFlagBitsEXT"},
        // length 32
        {"VK_ACCELERATION_STRUCTURE_TYPE_",          "vk::AccelerationStructureTypeKHR"},
        // length 31
        {"VK_VALIDATION_FEATURE_DISABLE_",           "vk::ValidationFeatureDisableEXT"},
        // length 30
        {"VK_VALIDATION_FEATURE_ENABLE_",            "vk::ValidationFeatureEnableEXT"},
        {"VK_TESSELLATION_DOMAIN_ORIGIN_",           "vk::TessellationDomainOrigin"},
        // length 29
        {"VK_DEBUG_UTILS_MESSAGE_TYPE_",             "vk::DebugUtilsMessageTypeFlagBitsEXT"},
        // length 27
        {"VK_LINE_RASTERIZATION_MODE_",              "vk::LineRasterizationModeEXT"},
        // length 26
        {"VK_PROVOKING_VERTEX_MODE_",                "vk::ProvokingVertexModeEXT"},
        // length 25
        {"VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_",      "vk::DescriptorUpdateTemplateType"},
        // length 24
        {"VK_COMMAND_BUFFER_LEVEL_",                 "vk::CommandBufferLevel"},
        {"VK_COMMAND_BUFFER_USAGE_",                 "vk::CommandBufferUsageFlagBits"},
        {"VK_COMMAND_BUFFER_RESET_",                 "vk::CommandBufferResetFlagBits"},
        {"VK_SAMPLER_ADDRESS_MODE_",                 "vk::SamplerAddressMode"},
        {"VK_PHYSICAL_DEVICE_TYPE_",                 "vk::PhysicalDeviceType"},
        // length 23
        {"VK_COMMAND_POOL_CREATE_",                  "vk::CommandPoolCreateFlagBits"},
        {"VK_SAMPLER_MIPMAP_MODE_",                  "vk::SamplerMipmapMode"},
        {"VK_ATTACHMENT_STORE_OP_",                  "vk::AttachmentStoreOp"},
        {"VK_PIPELINE_BIND_POINT_",                  "vk::PipelineBindPoint"},
        // length 22
        {"VK_COMMAND_POOL_RESET_",                   "vk::CommandPoolResetFlagBits"},
        {"VK_COMPONENT_SWIZZLE_",                    "vk::ComponentSwizzle"},
        {"VK_DESCRIPTOR_BINDING_",                   "vk::DescriptorBindingFlagBits"},
        {"VK_ATTACHMENT_LOAD_OP_",                   "vk::AttachmentLoadOp"},
        {"VK_PRIMITIVE_TOPOLOGY_",                   "vk::PrimitiveTopology"},
        // length 21
        {"VK_VERTEX_INPUT_RATE_",                    "vk::VertexInputRate"},
        {"VK_SWAPCHAIN_CREATE_",                     "vk::SwapchainCreateFlagBitsKHR"},
        {"VK_SURFACE_TRANSFORM_",                    "vk::SurfaceTransformFlagBitsKHR"},
        // length 20
        {"VK_PIPELINE_STAGE_2_",                     "vk::PipelineStageFlagBits2"},
        {"VK_COMPOSITE_ALPHA_",                      "vk::CompositeAlphaFlagBitsKHR"},
        {"VK_SUBPASS_CONTENTS_",                     "vk::SubpassContents"},
        {"VK_SUBGROUP_FEATURE_",                     "vk::SubgroupFeatureFlagBits"},
        // length 19
        {"VK_PIPELINE_CREATE_",                      "vk::PipelineCreateFlagBits"},
        {"VK_IMAGE_VIEW_TYPE_",                      "vk::ImageViewType"},
        {"VK_MEMORY_PROPERTY_",                      "vk::MemoryPropertyFlagBits"},
        {"VK_DESCRIPTOR_TYPE_",                      "vk::DescriptorType"},
        {"VK_COLOR_COMPONENT_",                      "vk::ColorComponentFlagBits"},
        // length 18
        {"VK_PIPELINE_STAGE_",                       "vk::PipelineStageFlagBits"},
        {"VK_FORMAT_FEATURE_",                       "vk::FormatFeatureFlagBits"},
        {"VK_BUFFER_USAGE_2_",                       "vk::BufferUsageFlagBits2"},
        {"VK_STRUCTURE_TYPE_",                       "vk::StructureType"},
        {"VK_SEMAPHORE_TYPE_",                       "vk::SemaphoreType"},
        {"VK_SEMAPHORE_WAIT_",                       "vk::SemaphoreWaitFlagBits"},
        // length 17
        {"VK_DYNAMIC_STATE_",                        "vk::DynamicState"},
        {"VK_BUFFER_CREATE_",                        "vk::BufferCreateFlagBits"},
        {"VK_GEOMETRY_TYPE_",                        "vk::GeometryTypeKHR"},
        // length 16
        {"VK_IMAGE_LAYOUT_",                         "vk::ImageLayout"},
        {"VK_IMAGE_ASPECT_",                         "vk::ImageAspectFlagBits"},
        {"VK_IMAGE_CREATE_",                         "vk::ImageCreateFlagBits"},
        {"VK_IMAGE_TILING_",                         "vk::ImageTiling"},
        {"VK_BUFFER_USAGE_",                         "vk::BufferUsageFlagBits"},
        {"VK_SHADER_STAGE_",                         "vk::ShaderStageFlagBits"},
        {"VK_SAMPLE_COUNT_",                         "vk::SampleCountFlagBits"},
        {"VK_SHARING_MODE_",                         "vk::SharingMode"},
        {"VK_POLYGON_MODE_",                         "vk::PolygonMode"},
        {"VK_BLEND_FACTOR_",                         "vk::BlendFactor"},
        {"VK_PRESENT_MODE_",                         "vk::PresentModeKHR"},
        {"VK_FENCE_CREATE_",                         "vk::FenceCreateFlagBits"},
        {"VK_QUERY_RESULT_",                         "vk::QueryResultFlagBits"},
        {"VK_RESOLVE_MODE_",                         "vk::ResolveModeFlags"},
        {"VK_BORDER_COLOR_",                         "vk::BorderColor"},
        // length 15
        {"VK_IMAGE_USAGE_",                          "vk::ImageUsageFlagBits"},
        {"VK_MEMORY_HEAP_",                          "vk::MemoryHeapFlagBits"},
        {"VK_COLOR_SPACE_",                          "vk::ColorSpaceKHR"},
        {"VK_OBJECT_TYPE_",                          "vk::ObjectType"},
        // length 14
        {"VK_IMAGE_TYPE_",                           "vk::ImageType"},
        {"VK_FRONT_FACE_",                           "vk::FrontFace"},
        {"VK_COMPARE_OP_",                           "vk::CompareOp"},
        {"VK_STENCIL_OP_",                           "vk::StencilOp"},
        {"VK_INDEX_TYPE_",                           "vk::IndexType"},
        {"VK_QUERY_TYPE_",                           "vk::QueryType"},
        {"VK_DEPENDENCY_",                           "vk::DependencyFlagBits"},
        // length 13
        {"VK_CULL_MODE_",                            "vk::CullModeFlagBits"},
        {"VK_RENDERING_",                            "vk::RenderingFlagBits"},
        // length 12
        {"VK_BLEND_OP_",                             "vk::BlendOp"},
        {"VK_LOGIC_OP_",                             "vk::LogicOp"},
        {"VK_ACCESS_2_",                             "vk::AccessFlagBits2"},
        // length 10
        {"VK_FORMAT_",                               "vk::Format"},
        {"VK_FILTER_",                               "vk::Filter"},
        {"VK_RESULT_",                               "vk::Result"},
        {"VK_ACCESS_",                               "vk::AccessFlagBits"},
        {"VK_SUBMIT_",                               "vk::SubmitFlagBits"},
        // length 9
        {"VK_QUEUE_",                                "vk::QueueFlagBits"},
    };

    // Vendor suffixes to strip from the value portion (must strip before _BIT)
    static constexpr std::string_view kVendorSuffixes[] = {
        "_KHR", "_EXT", "_NV", "_NVX", "_AMD", "_INTEL", "_GOOGLE",
        "_QCOM", "_HUAWEI", "_ARM", "_VALVE", "_MESA", "_FB", "_AMDX", "_ANDROID",
    };

    for (const auto& e : kPrefixes) {
        if (token.size() <= e.prefix.size()) continue;
        if (token.substr(0, e.prefix.size()) != e.prefix) continue;

        std::string value{token.substr(e.prefix.size())};

        // Strip vendor suffix from value
        for (std::string_view vendor : kVendorSuffixes) {
            if (value.size() > vendor.size() &&
                std::string_view{value}.substr(value.size() - vendor.size()) == vendor) {
                value.resize(value.size() - vendor.size());
                break;
            }
        }

        // Strip _BIT suffix
        if (value.size() >= 4 && std::string_view{value}.substr(value.size() - 4) == "_BIT")
            value.resize(value.size() - 4);

        // Split by '_' and convert each segment, prepend 'e'
        std::string enumValue = "e";
        std::size_t start = 0;
        while (start < value.size()) {
            std::size_t sep = value.find('_', start);
            if (sep == std::string::npos) sep = value.size();
            if (sep > start)
                enumValue += convertSegment(std::string_view{value}.substr(start, sep - start));
            start = sep + 1;
        }

        return std::string{e.cppType} + "::" + enumValue;
    }

    return {}; // no match
}

// Walks through the message and replaces every all-caps VK_ token it can
// recognise with its vk:: C++ equivalent.
static std::string translateVkNames(const std::string& msg) {
    std::string out;
    out.reserve(msg.size());

    std::size_t pos = 0;
    while (pos < msg.size()) {
        std::size_t vkPos = msg.find("VK_", pos);
        if (vkPos == std::string::npos) {
            out += msg.substr(pos);
            break;
        }
        out += msg.substr(pos, vkPos - pos);

        // Scan token: uppercase letters, digits, underscores only.
        // Stopping at lowercase avoids capturing extension/layer names like
        // "VK_KHR_swapchain" and mangling them.
        std::size_t end = vkPos + 3;
        while (end < msg.size()) {
            unsigned char c = static_cast<unsigned char>(msg[end]);
            if (std::isupper(c) || std::isdigit(c) || c == '_') ++end;
            else break;
        }
        // Trim trailing underscores from the candidate token
        while (end > vkPos + 3 && msg[end - 1] == '_') --end;

        std::string_view token{msg.data() + vkPos, end - vkPos};
        std::string converted = tryConvertVkName(token);
        out += converted.empty() ? std::string{token} : converted;
        pos = end;
    }

    return out;
}

// ---------------------------------------------------------------------------
// SwLogger
// ---------------------------------------------------------------------------

SwLogger::SwLogger() {
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

    quill::ConsoleSinkConfig::Colours consoleColours;
    consoleColours.assign_colour_to_log_level(quill::LogLevel::Error, "\033[38;5;208m");
    quill::ConsoleSinkConfig consoleSinkConfig;
    consoleSinkConfig.set_colours(consoleColours);
    consoleSinkConfig.set_colour_mode(quill::ConsoleSinkConfig::ColourMode::Automatic);
    auto consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink1", consoleSinkConfig);

    quill::PatternFormatterOptions options{};
    options.format_pattern = "\n[%(short_source_location) | %(time) | %(log_level)] %(message)";
    options.timestamp_pattern = "%H:%M:%S.%Qms";
    options.add_metadata_to_multi_line_logs = false;

    switch (LOG_LOCATION) {
        case LogLocation::File:
            mLogger = quill::Frontend::create_or_get_logger("LOGGER", {std::move(fileSink), std::move(latestFileSink)}, options);
            break;
        case LogLocation::Console:
            mLogger = quill::Frontend::create_or_get_logger("LOGGER", std::move(consoleSink), options);
            break;
        case LogLocation::Both:
            mLogger = quill::Frontend::create_or_get_logger("LOGGER", {std::move(fileSink), std::move(latestFileSink), std::move(consoleSink)}, options);
            break;
    }

    mLogger->set_log_level(LOG_LEVEL);

    mBlockedMessages.insert("BestPractices-vkBindBufferMemory-small-dedicated-allocation");
    mBlockedMessages.insert("BestPractices-vkBindImageMemory-small-dedicated-allocation");

    mBreakMessages.insert("VUID-VkBufferCopy-size-01988");
}

VKAPI_ATTR VkBool32 VKAPI_CALL SwLogger::debugMessageFunc(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
) {
    auto* swLogger = static_cast<SwLogger*>(pUserData);

    const std::string messageName{pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : ""};
    if (swLogger->mBlockedMessages.contains(messageName)) return vk::False;
    if (swLogger->mBreakMessages.contains(messageName)) SW_DEBUG_BREAK();

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
        swLogger->getFrameNumber(),
        pCallbackData->pMessage,
        queueLabels,
        cmdBufLabels,
        resources
    );

    message = translateVkNames(message);

    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            LOG_ERROR(swLogger->getLogger(), "{}", message);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            LOG_WARNING(swLogger->getLogger(), "{}", message);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            LOG_TRACE_L3(swLogger->getLogger(), "{}", message);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        default:
            break;
    }

    return vk::False;
}
