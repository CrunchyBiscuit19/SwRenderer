#include <Pass/Cull/SwCullReset.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwShader.h>

std::filesystem::path SwCullResetPass::CULL_RESET_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerReset.comp.spv";

void SwCullResetPass::initialize() { 
	
}
