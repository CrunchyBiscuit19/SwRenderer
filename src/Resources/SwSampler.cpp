#include <Renderer/SwRenderer.h>
#include <Resources/SwSampler.h>

SwFactoryContext SwSamplerFactory::sRendererContext{};

SwSampler::SwSampler(vk::raii::Sampler sampler) : mSampler(std::move(sampler)) {}

void SwSamplerFactory::init(SwFactoryContext rendererContext) { sRendererContext = rendererContext; }

SwSampler SwSamplerFactory::createSampler(vk::SamplerCreateInfo samplerCreateInfo) {
    return SwSampler(sRendererContext.mDevice->createSampler(samplerCreateInfo));
}