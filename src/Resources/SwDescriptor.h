#pragma once

#include <vk_mem_alloc.h>

#include <vulkan/vulkan_raii.hpp>

struct SwFactoryContext;

class SwDescriptorLayout {
private:
    vk::raii::DescriptorSetLayout mLayout;
    std::vector<vk::DescriptorSetLayoutBinding> mBindings;

public:
    SwDescriptorLayout(vk::raii::DescriptorSetLayout layout, std::vector<vk::DescriptorSetLayoutBinding> bindings);

    SwDescriptorLayout(SwDescriptorLayout&&) noexcept = default;
    SwDescriptorLayout& operator=(SwDescriptorLayout&&) noexcept = default;

    SwDescriptorLayout(const SwDescriptorLayout&) = delete;
    SwDescriptorLayout& operator=(const SwDescriptorLayout&) = delete;

    inline vk::DescriptorSetLayout getRawLayout() { return *mLayout; };

    inline std::span<const vk::DescriptorSetLayoutBinding> getBindings() const { return mBindings; };
};

class SwDescriptorSet {
private:
    vk::raii::DescriptorSet mSet;
    std::span<const vk::DescriptorSetLayoutBinding> mBindings;
    std::vector<vk::WriteDescriptorSet> mWrites;

public:
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

class SwDescriptorPool {
public:
    struct SwPoolSizeRatio {
        vk::DescriptorType mType;
        float mRatio;
    };

private:
    static SwFactoryContext sRendererContext;
    static const std::uint32_t MAX_SETS_PER_POOL{4096};

    std::vector<SwPoolSizeRatio> mRatios;
    std::uint32_t mSetsPerPool;
    std::vector<vk::raii::DescriptorPool> mReadyPools;
    std::vector<vk::raii::DescriptorPool> mFullPools;

    vk::raii::DescriptorPool createPool(std::uint32_t setCount) const;

    vk::raii::DescriptorPool& getPool();

    static std::uint32_t nextPoolSize(std::uint32_t current);

public:
    SwDescriptorPool(std::vector<SwPoolSizeRatio> ratios, std::uint32_t setsPerPool);

    SwDescriptorPool(SwDescriptorPool&&) noexcept = default;
    SwDescriptorPool& operator=(SwDescriptorPool&&) noexcept = default;

    SwDescriptorPool(const SwDescriptorPool&) = delete;
    SwDescriptorPool& operator=(const SwDescriptorPool&) = delete;

    static void init(SwFactoryContext rendererContext);

    SwDescriptorLayout createDescriptorLayout(
        std::vector<vk::DescriptorSetLayoutBinding> bindings, vk::ShaderStageFlags shaderStages, bool useBindless = false
    );

    SwDescriptorSet createDescriptorSet(SwDescriptorLayout layout);

    void resetPools();

    void clearPools();
};