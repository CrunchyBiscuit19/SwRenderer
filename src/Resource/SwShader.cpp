#include <Renderer/SwRenderer.h>
#include <Resource/SwShader.h>

#include <bit>
#include <fstream>

SwShader::SwShader() : mModule(nullptr), mStage(vk::ShaderStageFlagBits::eVertex) {}

SwShader::SwShader(vk::raii::ShaderModule module, vk::ShaderStageFlagBits shaderStageFlag) : mModule(std::move(module)), mStage(shaderStageFlag) {}

void SwShader::destroy() { mModule.clear(); }

SwRendererContext SwShaderFactory::sRendererContext{};

void SwShaderFactory::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

SwShader SwShaderFactory::createShader(const std::filesystem::path& filePath, vk::ShaderStageFlagBits shaderStageFlag) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    const size_t fileSize = file.tellg();
    std::vector<std::uint32_t> buffer(fileSize / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
    file.close();

    vk::ShaderModuleCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.pNext = nullptr;
    shaderCreateInfo.codeSize = buffer.size() * sizeof(std::uint32_t);
    shaderCreateInfo.pCode = buffer.data();

    return SwShader(sRendererContext.mDevice->createShaderModule(shaderCreateInfo), shaderStageFlag);
}