#include <Resources/SwSampler.h>

SwRendererContext SwSamplerFactory::sRendererContext{};

SwSampler::SwSampler(vk::raii::Sampler sampler) : mSampler(sampler) {}

void SwSamplerFactory::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

SwSampler SwSamplerFactory::createSampler(vk::SamplerCreateInfo samplerCreateInfo) { return SwSampler(sRendererContext.mDevice->createSampler(samplerCreateInfo)); }