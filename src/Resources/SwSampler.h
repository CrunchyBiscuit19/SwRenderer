#pragma once

#include <Renderer/SwRenderer.h>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan_raii.hpp>

class SwSampler {
private:
    vk::raii::Sampler mSampler;

public:
    SwSampler(vk::raii::Sampler);

    inline vk::Sampler getRawSampler() { return *mSampler; }
};

class SwSamplerFactory {
private:
    static SwRendererContext sRendererContext;

public:
    static void init(SwRendererContext rendererContext);

    static SwSampler createSampler(vk::SamplerCreateInfo);
};