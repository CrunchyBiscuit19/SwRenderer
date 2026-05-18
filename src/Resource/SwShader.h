#pragma once

#include <vk_mem_alloc.h>

#include <filesystem>
#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwShader {
private:
    vk::raii::ShaderModule mModule;
    vk::ShaderStageFlagBits mStage;

public:
    SwShader();

    SwShader(vk::raii::ShaderModule, vk::ShaderStageFlagBits);

    inline vk::ShaderModule getRawModule() { return *mModule; }

    void destroy();
};

class SwShaderFactory {
private:
    static SwRendererContext sRendererContext;

public:
    static void init(SwRendererContext rendererContext);

    static SwShader createShader(const std::filesystem::path& filePath, vk::ShaderStageFlagBits shaderStageFlag);
};