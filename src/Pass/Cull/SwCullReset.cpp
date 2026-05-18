#include <Pass/Cull/SwCullReset.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwShader.h>

std::filesystem::path SwCullResetPass::CULL_RESET_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerReset.comp.spv";

void SwCullResetPass::initialize() { 
	vk::PushConstantRange resetPushConstantRange{};
    resetPushConstantRange.offset = 0;
    resetPushConstantRange.size = sizeof(SwCullResetPushConstants);
    resetPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
	
	mResetPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, resetPushConstantRange); 

	SwShader computeShaderModule = SwShaderFactory::createShader(CULL_RESET_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);

    mResetPipelinePipeline = SwComputePipelineFactory::createComputePipeline({computeShaderModule.getRawModule(), mResetPipelineLayout.getRawLayout()});    
}
