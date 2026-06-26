#include <Renderer/SwRenderer.h>
#include <Resource/SwSampler.h>

SwSampler SwSampler::sDefaultSampler{};

SwSampler::SwSampler(): mSampler(nullptr) {}

SwSampler::SwSampler(vk::raii::Sampler sampler) : mSampler(std::move(sampler)) {}

void SwSamplerFactory::init() {
    vk::SamplerCreateInfo defaultSamplerCreateInfo{};
    defaultSamplerCreateInfo.magFilter = vk::Filter::eLinear;
    defaultSamplerCreateInfo.minFilter = vk::Filter::eLinear;
    defaultSamplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    defaultSamplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    defaultSamplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    defaultSamplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    defaultSamplerCreateInfo.minLod = 0.0f;
    defaultSamplerCreateInfo.maxLod = vk::LodClampNone;
    defaultSamplerCreateInfo.anisotropyEnable = vk::True;
    defaultSamplerCreateInfo.maxAnisotropy = getMaxSamplerAnisotropy();
    SwSampler::sDefaultSampler = createSampler("DefaultSampler", defaultSamplerCreateInfo);
}

float SwSamplerFactory::getMaxSamplerAnisotropy() {
    return SwRenderer::sRendererContext.mChosenGPU->getProperties().limits.maxSamplerAnisotropy;
}

void SwSamplerFactory::cleanup() { SwSampler::sDefaultSampler = SwSampler{}; }

SwSampler SwSamplerFactory::createSampler(std::string name, vk::SamplerCreateInfo samplerCreateInfo) {
    SwSampler sampler(SwRenderer::sRendererContext.mDevice->createSampler(samplerCreateInfo));
    SwRenderer::sRendererContext.labelResourceDebug(sampler.getHandle(), name.c_str());
    return sampler;
}