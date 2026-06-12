#pragma once

#include <vk_mem_alloc.h>

#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

struct SwSamplerOptions {
    vk::Filter magFilter;
    vk::Filter minFilter;
    vk::SamplerMipmapMode mipmapMode;
    vk::SamplerAddressMode addressModeU;
    vk::SamplerAddressMode addressModeV;
    vk::SamplerAddressMode addressModeW;
    const void* pNext;

    SwSamplerOptions()
        : magFilter(vk::Filter::eLinear),
          minFilter(vk::Filter::eLinear),
          mipmapMode(vk::SamplerMipmapMode::eLinear),
          addressModeU(vk::SamplerAddressMode::eRepeat),
          addressModeV(vk::SamplerAddressMode::eRepeat),
          addressModeW(vk::SamplerAddressMode::eRepeat),
          pNext(nullptr) {}

    SwSamplerOptions(vk::SamplerCreateInfo samplerCreateInfo)
        : magFilter(samplerCreateInfo.magFilter),
          minFilter(samplerCreateInfo.minFilter),
          mipmapMode(samplerCreateInfo.mipmapMode),
          addressModeU(samplerCreateInfo.addressModeU),
          addressModeV(samplerCreateInfo.addressModeV),
          addressModeW(samplerCreateInfo.addressModeW),
          pNext(samplerCreateInfo.pNext) {}

    bool operator==(const SwSamplerOptions& other) const {
        return (
            magFilter == other.magFilter && minFilter == other.minFilter && mipmapMode == other.mipmapMode && addressModeU == other.addressModeU &&
            addressModeV == other.addressModeV && addressModeW == other.addressModeW && pNext == other.pNext
        );
    }
};

template <>
struct std::hash<SwSamplerOptions> {
    // Compute individual hash values for strings
    // Combine them using XOR and bit shifting
    std::size_t operator()(const SwSamplerOptions& k) const {
        std::size_t seed = 0;
        hashCombine(seed, static_cast<std::uint32_t>(k.magFilter));
        hashCombine(seed, static_cast<std::uint32_t>(k.minFilter));
        hashCombine(seed, static_cast<std::uint32_t>(k.mipmapMode));
        hashCombine(seed, static_cast<std::uint32_t>(k.addressModeU));
        hashCombine(seed, static_cast<std::uint32_t>(k.addressModeV));
        hashCombine(seed, static_cast<std::uint32_t>(k.addressModeW));
        return seed;
    }

    static void hashCombine(std::size_t& seed, std::size_t value) {
        std::hash<std::size_t> hasher;
        seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
};

class SwSampler {
private:
    vk::raii::Sampler mSampler;

public:
    static SwSampler sDefaultSampler;

    SwSampler();

    SwSampler(vk::raii::Sampler);

    inline vk::Sampler getRawSampler() { return *mSampler; }
};

class SwSamplerFactory {
private:

public:
    static void init();

    static void cleanup();

    static SwSampler createSampler(std::string name, vk::SamplerCreateInfo);
};