#pragma once

#include <vk_mem_alloc.h>

#include <vulkan/vulkan_raii.hpp>

struct SwFactoryContext;

class SwSampler {
private:
    vk::raii::Sampler mSampler;

public:
    SwSampler(vk::raii::Sampler);

    inline vk::Sampler getRawSampler() { return *mSampler; }
};

class SwSamplerFactory {
private:
    static SwFactoryContext sRendererContext;

public:
    static void init(SwFactoryContext rendererContext);

    static SwSampler createSampler(vk::SamplerCreateInfo);
};