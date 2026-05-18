#pragma once

#include <Resource/SwPipeline.h>

#include <vulkan/vulkan.hpp>

#include <filesystem>

struct SwCullResetPushConstants {
    vk::DeviceAddress mPreCullRenderItemsBuffer;
    std::uint32_t mPreCullRenderItemsLimit;
};

class SwCullResetPass {
private:
    static std::filesystem::path CULL_RESET_COMPUTE_SHADER_PATH;

    SwPipelinePipeline mResetPipelinePipeline;
    SwPipelineLayout mResetPipelineLayout;
    SwCullResetPushConstants mResetPushConstants;

public:
    SwCullResetPass() = default;
    
    void initialize();
};