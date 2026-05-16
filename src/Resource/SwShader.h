#pragma once

#include <vk_mem_alloc.h>

#include <filesystem>
#include <vulkan/vulkan_raii.hpp>

struct SwFactoryContext;

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
    static SwFactoryContext sRendererContext;

public:
    static void init(SwFactoryContext rendererContext);

    static SwShader createShader(const std::filesystem::path& filePath, vk::ShaderStageFlagBits shaderStageFlag);
};