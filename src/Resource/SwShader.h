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

    inline vk::ShaderModule getHandle() { return *mModule; }

    void destroy();
};

class SwShaderFactory {
private:

public:
    static void init();

    static SwShader createShader(std::string name, const std::filesystem::path& filePath, vk::ShaderStageFlagBits shaderStageFlag);
};