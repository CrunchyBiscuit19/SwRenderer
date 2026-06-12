#pragma once

#include <vk_mem_alloc.h>

#include <deque>
#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwDescriptorLayout {
private:
    vk::raii::DescriptorSetLayout mLayout;
    std::vector<vk::DescriptorSetLayoutBinding> mBindings;
    bool mUseBindless;

public:
    SwDescriptorLayout();

    SwDescriptorLayout(vk::raii::DescriptorSetLayout layout, std::vector<vk::DescriptorSetLayoutBinding> bindings, bool useBindless);

    SwDescriptorLayout(SwDescriptorLayout&&) noexcept = default;
    SwDescriptorLayout& operator=(SwDescriptorLayout&&) noexcept = default;

    SwDescriptorLayout(const SwDescriptorLayout&) = delete;
    SwDescriptorLayout& operator=(const SwDescriptorLayout&) = delete;

    inline vk::DescriptorSetLayout getRawLayout() { return *mLayout; };
    inline std::span<const vk::DescriptorSetLayoutBinding> getBindings() const { return mBindings; };
    inline bool usesBindless() const { return mUseBindless; };

    void destroy();
};

class SwDescriptorSet {
private:
    vk::raii::DescriptorSet mSet;
    std::span<const vk::DescriptorSetLayoutBinding> mBindings;
    std::vector<vk::WriteDescriptorSet> mWrites;
    std::deque<vk::DescriptorImageInfo> mWriteImageInfos;
    std::deque<vk::DescriptorBufferInfo> mWriteBufferInfos;
    bool mUseBindless;

public:
    SwDescriptorSet();

    SwDescriptorSet(vk::raii::DescriptorSet mSet, std::span<const vk::DescriptorSetLayoutBinding> bindings, bool useBindless);

    void writeImage(
        std::uint32_t binding, vk::ImageView imageView, vk::Sampler sampler, vk::ImageLayout layout, std::uint32_t arrayIndex = 0
    );

    void writeSampler(std::uint32_t binding, vk::Sampler sampler);

    void writeBuffer(std::uint32_t binding, vk::Buffer buffer, size_t size, size_t offset);

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
    static constexpr std::uint32_t MAX_SETS_PER_POOL{1 << 12};

    std::vector<SwPoolSizeRatio> mRatios;
    std::uint32_t mSetsPerPool;
    std::vector<SwDescriptorPool> mReadyPools;
    std::vector<SwDescriptorPool> mFullPools;

    SwDescriptorPool createPool(std::uint32_t setCount) const;

    SwDescriptorPool& getPool();

    static std::uint32_t nextPoolSize(std::uint32_t current);

public:
    SwDescriptorAllocator(std::vector<SwPoolSizeRatio> ratios, std::uint32_t setsPerPool);

    SwDescriptorAllocator(SwDescriptorAllocator&&) noexcept = default;
    SwDescriptorAllocator& operator=(SwDescriptorAllocator&&) noexcept = default;

    SwDescriptorAllocator(const SwDescriptorAllocator&) = delete;
    SwDescriptorAllocator& operator=(const SwDescriptorAllocator&) = delete;

    static void init();

    SwDescriptorPool createDescriptorPool(vk::ArrayProxy<SwPoolSizeRatio> ratios, std::uint32_t setsPerPool);

    SwDescriptorLayout createDescriptorLayout(
        std::string name, std::vector<vk::DescriptorSetLayoutBinding> bindings, vk::ShaderStageFlags shaderStages, bool useBindless = false
    );

    SwDescriptorSet createDescriptorSet(std::string name, SwDescriptorLayout& layout, std::uint32_t bindlessDescriptorCount = 0);

    void resetPools();

    void clearPools();
};