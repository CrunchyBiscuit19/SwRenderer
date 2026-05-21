#include <Renderer/SwRenderer.h>
#include <Resource/SwDescriptor.h>

SwDescriptorLayout::SwDescriptorLayout() : mLayout(nullptr) {};

SwDescriptorLayout::SwDescriptorLayout(vk::raii::DescriptorSetLayout layout, std::vector<vk::DescriptorSetLayoutBinding> bindings, bool bindless)
    : mLayout(std::move(layout)), mBindings(std::move(bindings)), mUseBindless(bindless) {}

void SwDescriptorLayout::destroy() { mLayout.clear(); }

SwDescriptorSet::SwDescriptorSet() : mSet(nullptr) {};

SwDescriptorSet::SwDescriptorSet(vk::raii::DescriptorSet set, std::span<const vk::DescriptorSetLayoutBinding> bindings, bool useBindless)
    : mSet(std::move(set)), mBindings(bindings), mUseBindless(useBindless) {}

void SwDescriptorSet::writeImage(
    std::uint32_t bindingIndex, vk::ImageView imageView, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type, std::uint32_t arrayIndex
) {
    if (mBindings[bindingIndex].descriptorType != type) {
        throw std::runtime_error("Descriptor type mismatch when writing image to descriptor set");
    };
    mWriteImageInfos.emplace_back(sampler, imageView, layout);
    mWrites.emplace_back(*mSet, bindingIndex, arrayIndex, 1, type, &mWriteImageInfos.back());
}

void SwDescriptorSet::writeSampler(std::uint32_t bindingIndex, vk::Sampler sampler, vk::DescriptorType type) {
    if (mBindings[bindingIndex].descriptorType != type) {
        throw std::runtime_error("Descriptor type mismatch when writing sampler to descriptor set");
    };
    mWriteImageInfos.emplace_back(sampler);
    mWrites.emplace_back(*mSet, bindingIndex, 0, 1, type, &mWriteImageInfos.back());
}

void SwDescriptorSet::writeBuffer(std::uint32_t bindingIndex, vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type) {
    if (mBindings[bindingIndex].descriptorType != type) {
        throw std::runtime_error("Descriptor type mismatch when writing buffer to descriptor set");
    };
    mWriteBufferInfos.emplace_back(buffer, offset, size);
    mWrites.emplace_back(*mSet, bindingIndex, 0, 1, type, nullptr, &mWriteBufferInfos.back());
}

void SwDescriptorSet::pushWrites() {
    if (mWrites.empty()) return;
    mSet.getDevice().updateDescriptorSets(mWrites, {});
    mWrites.clear();
}

void SwDescriptorSet::clearWrites() { mWrites.clear(); }

SwDescriptorPool::SwDescriptorPool(vk::raii::DescriptorPool descriptorPool) : mPool(std::move(descriptorPool)) {}

SwRendererContext SwDescriptorAllocator::sRendererContext{};

SwDescriptorAllocator::SwDescriptorAllocator(std::vector<SwPoolSizeRatio> ratios, std::uint32_t setsPerPool)
    : mRatios(std::move(ratios)), mSetsPerPool(setsPerPool) {}

void SwDescriptorAllocator::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

SwDescriptorPool SwDescriptorAllocator::createPool(std::uint32_t setCount) const {
    std::vector<vk::DescriptorPoolSize> sizes;
    sizes.reserve(mRatios.size());
    for (const auto& r : mRatios) {
        sizes.emplace_back(r.mType, static_cast<std::uint32_t>(r.mRatio * setCount));
    }

    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descriptorPoolCreateInfo.maxSets = setCount;
    descriptorPoolCreateInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
    descriptorPoolCreateInfo.pPoolSizes = sizes.data();

    return SwDescriptorPool(vk::raii::DescriptorPool(*sRendererContext.mDevice, descriptorPoolCreateInfo));
}

SwDescriptorPool& SwDescriptorAllocator::getPool() {
    if (mReadyPools.empty()) {
        mReadyPools.emplace_back(std::move(createPool(mSetsPerPool)));
    }
    return mReadyPools.back();
}

std::uint32_t SwDescriptorAllocator::nextPoolSize(std::uint32_t current) { return std::min(current * 2, MAX_SETS_PER_POOL); }

SwDescriptorPool SwDescriptorAllocator::createDescriptorPool(vk::ArrayProxy<SwPoolSizeRatio> ratios, std::uint32_t setCount) {
    std::vector<vk::DescriptorPoolSize> sizes;
    sizes.reserve(ratios.size());
    for (const auto& r : ratios) {
        sizes.emplace_back(r.mType, static_cast<std::uint32_t>(r.mRatio * setCount));
    }

    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descriptorPoolCreateInfo.maxSets = setCount;
    descriptorPoolCreateInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
    descriptorPoolCreateInfo.pPoolSizes = sizes.data();

    return SwDescriptorPool(vk::raii::DescriptorPool(*sRendererContext.mDevice, descriptorPoolCreateInfo));
}

SwDescriptorLayout SwDescriptorAllocator::createDescriptorLayout(
    std::vector<vk::DescriptorSetLayoutBinding> bindings, vk::ShaderStageFlags shaderStages, bool useBindless
) {
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    vk::DescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo{};
    descriptorLayoutCreateInfo.pBindings = bindings.data();
    descriptorLayoutCreateInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());

    vk::DescriptorBindingFlags bindlessFlags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                                               vk::DescriptorBindingFlagBits::eVariableDescriptorCount;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo descriptorLayoutBindingFlagsCreateInfo{};
    descriptorLayoutBindingFlagsCreateInfo.pBindingFlags = &bindlessFlags;
    descriptorLayoutBindingFlagsCreateInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());

    if (useBindless) {
        descriptorLayoutCreateInfo.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
        descriptorLayoutCreateInfo.pNext = &descriptorLayoutBindingFlagsCreateInfo;
    }

    return SwDescriptorLayout(sRendererContext.mDevice->createDescriptorSetLayout(descriptorLayoutCreateInfo, nullptr), std::move(bindings), useBindless);
}

SwDescriptorSet SwDescriptorAllocator::createDescriptorSet(SwDescriptorLayout& layout, std::uint32_t bindlessDescriptorCount) {
    if (mReadyPools.empty()) {
        mReadyPools.emplace_back(std::move(createPool(mSetsPerPool)));
    }

    vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.descriptorPool = getPool().getRawPool();
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    auto rawLayout = layout.getRawLayout();
    descriptorSetAllocateInfo.pSetLayouts = &rawLayout;

    vk::DescriptorSetVariableDescriptorCountAllocateInfo countInfo;
    countInfo.descriptorSetCount = 1;
    countInfo.pDescriptorCounts = &bindlessDescriptorCount;
    if (layout.usesBindless()) descriptorSetAllocateInfo.pNext = &countInfo;

    try {
        auto sets = sRendererContext.mDevice->allocateDescriptorSets(descriptorSetAllocateInfo);
        return SwDescriptorSet(std::move(sets.front()), layout.getBindings(), layout.usesBindless());
    } catch (const vk::OutOfPoolMemoryError&) {
        /* grow below */
    } catch (const vk::FragmentedPoolError&) {
        /* grow below */
    }

    mFullPools.emplace_back(std::move(mReadyPools.back()));
    mReadyPools.pop_back();
    mSetsPerPool = nextPoolSize(mSetsPerPool);
    mReadyPools.emplace_back(std::move(createPool(mSetsPerPool)));
    descriptorSetAllocateInfo.descriptorPool = mReadyPools.back().getRawPool();

    auto sets = sRendererContext.mDevice->allocateDescriptorSets(descriptorSetAllocateInfo);
    return SwDescriptorSet(std::move(sets.front()), layout.getBindings(), layout.usesBindless());
}

void SwDescriptorAllocator::resetPools() {
    for (auto& pool : mReadyPools) {
        pool.reset();
    }
    for (auto& pool : mFullPools) {
        pool.reset();
        mReadyPools.emplace_back(std::move(pool));
    }
    mFullPools.clear();
}

void SwDescriptorAllocator::clearPools() {
    mReadyPools.clear();
    mFullPools.clear();
}
