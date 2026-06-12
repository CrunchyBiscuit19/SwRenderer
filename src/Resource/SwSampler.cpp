#include <Renderer/SwRenderer.h>
#include <Resource/SwSampler.h>

SwSampler SwSampler::sDefaultSampler{};

SwSampler::SwSampler(): mSampler(nullptr) {}

SwSampler::SwSampler(vk::raii::Sampler sampler) : mSampler(std::move(sampler)) {}

void SwSamplerFactory::init() {
    SwSampler::sDefaultSampler = createSampler("DefaultSampler", vk::SamplerCreateInfo{});
}

void SwSamplerFactory::cleanup() { SwSampler::sDefaultSampler = SwSampler{}; }

SwSampler SwSamplerFactory::createSampler(std::string name, vk::SamplerCreateInfo samplerCreateInfo) {
    SwSampler sampler(SwRenderer::sRendererContext.mDevice->createSampler(samplerCreateInfo));
    SwRenderer::sRendererContext.labelResourceDebug(sampler.getRawSampler(), name.c_str());
    return sampler;
}