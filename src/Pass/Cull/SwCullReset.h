#pragma once

#include <Resource/SwPipeline.h>
#include <Pass/SwPass.h>

#include <vulkan/vulkan.hpp>

#include <filesystem>


class SwCullResetPass: public SwPass {
private:
    static std::filesystem::path CULL_RESET_COMPUTE_SHADER_PATH;



public:
    SwCullResetPass() = default;

    SwCullResetPass(
        std::string name, std::vector<SwImageDep> readImageDeps, std::vector<SwImageDep> writeImageDeps, std::vector<SwBufferDep> readBufferDeps,
        std::vector<SwBufferDep> writeBufferDeps, std::function<void(vk::CommandBuffer)> callback, bool mustRun = false
    );
    
    void initialize();
};