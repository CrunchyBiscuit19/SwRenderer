#pragma once

#include <vk_mem_alloc.h>

#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwDescriptorLayout {
private:
    vk::raii::DescriptorSetLayout mLayout;
    std::vector<vk::DescriptorSetLayoutBinding> mBindings;

public:
    SwDescriptorLayout();

    SwDescriptorLayout(vk::raii::DescriptorSetLayout layout, std::vector<vk::DescriptorSetLayoutBinding> bindings);

    SwDescriptorLayout(SwDescriptorLayout&&) noexcept = default;
    SwDescriptorLayout& operator=(SwDescriptorLayout&&) noexcept = default;

    SwDescriptorLayout(const SwDescriptorLayout&) = delete;
    SwDescriptorLayout& operator=(const SwDescriptorLayout&) = delete;

    inline vk::DescriptorSetLayout getRawLayout() { return *mLayout; };

    inline std::span<const vk::DescriptorSetLayoutBinding> getBindings() const { return mBindings; };

    void destroy();
};

class SwDescriptorSet {
private:
    vk::raii::DescriptorSet mSet;
    std::span<const vk::DescriptorSetLayoutBinding> mBindings;
    std::vector<vk::WriteDescriptorSet> mWrites;

public:
    SwDescriptorSet();

    SwDescriptorSet(vk::raii::DescriptorSet mSet, std::span<const vk::DescriptorSetLayoutBinding> bindings);

    void writeImage(
        std::uint32_t binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type, std::uint32_t arrayIndex = 0
    );

    void writeSampler(std::uint32_t binding, vk::Sampler sampler, vk::DescriptorType type);

    void writeBuffer(std::uint32_t binding, vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type);

    void pushWrites();

    void clearWrites();

    inline vk::DescriptorSet getRawSet() { return *mSet; };

    SwDescriptorSet(SwDescriptorSet&&) noexcept = default;
    SwDescriptorSet& operator=(SwDescriptorSet&&) noexcept = default;

    SwDescriptorSet(const SwDescriptorSet&) = delete;
    SwDescriptorSet& operator=(const SwDescriptorSet&) = delete;
};

struct SwPoolSizeRatio {
    vk::DescriptorType mType;
    float mRatio;
};

class SwDescriptorPool {
private:
    vk::raii::DescriptorPool mPool;

public:
    SwDescriptorPool() : mPool(nullptr) {}

    SwDescriptorPool(vk::raii::DescriptorPool);

    inline vk::DescriptorPool getRawPool() { return *mPool; }

    inline void reset() { mPool.reset(); };
};

class SwDescriptorAllocator {
private:
    static SwRendererContext sRendererContext;
    static const std::uint32_t MAX_SETS_PER_POOL{4096};

    std::vector<SwPoolSizeRatio> mRatios;
    std::uint32_t mSetsPerPool;
    std::vector<SwDescriptorPool> mReadyPools;
    std::vector<SwDescriptorPool> mFullPools;

    SwDescriptorPool createPool(std::uint32_t setCount) const;

    SwDescriptorPool& getPool();

    static std::uint32_t nextPoolSize(std::uint32_t current);

public:
    SwDescriptorAllocator();

    SwDescriptorAllocator(std::vector<SwPoolSizeRatio> ratios, std::uint32_t setsPerPool);

    SwDescriptorAllocator(SwDescriptorAllocator&&) noexcept = default;
    SwDescriptorAllocator& operator=(SwDescriptorAllocator&&) noexcept = default;

    SwDescriptorAllocator(const SwDescriptorAllocator&) = delete;
    SwDescriptorAllocator& operator=(const SwDescriptorAllocator&) = delete;

    static void init(SwRendererContext rendererContext);

    SwDescriptorPool createDescriptorPool(vk::ArrayProxy<SwPoolSizeRatio> ratios, std::uint32_t setsPerPool);

    SwDescriptorLayout createDescriptorLayout(
        std::vector<vk::DescriptorSetLayoutBinding> bindings, vk::ShaderStageFlags shaderStages, bool useBindless = false
    );

    SwDescriptorSet createDescriptorSet(SwDescriptorLayout& layout);

    void resetPools();

    void clearPools();
};