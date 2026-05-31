#include <Renderer/SwRenderer.h>
#include <Resource/SwSampler.h>

SwRendererContext SwSamplerFactory::sRendererContext{};
SwSampler SwSampler::sDefaultSampler{};

SwSampler::SwSampler(): mSampler(nullptr) {}

SwSampler::SwSampler(vk::raii::Sampler sampler) : mSampler(std::move(sampler)) {}

void SwSamplerFactory::init(SwRendererContext rendererContext) {
    sRendererContext = rendererContext;
    SwSampler::sDefaultSampler = createSampler(vk::SamplerCreateInfo{});
}

void SwSamplerFactory::cleanup() { SwSampler::sDefaultSampler = SwSampler{}; }

SwSampler SwSamplerFactory::createSampler(vk::SamplerCreateInfo samplerCreateInfo) {
    return SwSampler(sRendererContext.mDevice->createSampler(samplerCreateInfo));
}