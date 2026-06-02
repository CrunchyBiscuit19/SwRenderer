#include <Renderer/SwRenderer.h>
#include <Resource/SwSampler.h>

SwSampler SwSampler::sDefaultSampler{};

SwSampler::SwSampler(): mSampler(nullptr) {}

SwSampler::SwSampler(vk::raii::Sampler sampler) : mSampler(std::move(sampler)) {}

void SwSamplerFactory::init() {
    SwSampler::sDefaultSampler = createSampler(vk::SamplerCreateInfo{});
}

void SwSamplerFactory::cleanup() { SwSampler::sDefaultSampler = SwSampler{}; }

SwSampler SwSamplerFactory::createSampler(vk::SamplerCreateInfo samplerCreateInfo) {
    return SwSampler(SwRenderer::sRendererContext.mDevice->createSampler(samplerCreateInfo));
}