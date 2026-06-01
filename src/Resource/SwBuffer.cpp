#include <Renderer/SwRenderer.h>
#include <Resource/SwBuffer.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

SwBuffer::SwBuffer()
    : mBuffer(nullptr),
      mAllocator(nullptr),
      mAllocation(nullptr),
      mFlags(0),
      mUsage(0),
      mSize(0),
      mCurrentStage(vk::PipelineStageFlagBits2::eTopOfPipe),
      mCurrentAccess(vk::AccessFlags2()) {}

SwBuffer::SwBuffer(
    vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info,
    vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint32_t size
)
    : mBuffer(std::move(buffer)),
      mAddress(address),
      mAllocator(allocator),
      mAllocation(allocation),
      mInfo(info),
      mFlags(flags),
      mUsage(usage),
      mSize(size),
      mCurrentStage(vk::PipelineStageFlagBits2::eTopOfPipe),
      mCurrentAccess(vk::AccessFlags2()) {}

void SwBuffer::emitBarrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess) {
    if (nextStage == mCurrentStage && nextAccess == mCurrentAccess) {
        return;
    }

    vk::BufferMemoryBarrier2 barrierInfo = {};
    barrierInfo.pNext = nullptr;
    barrierInfo.srcStageMask = mCurrentStage;
    barrierInfo.dstStageMask = nextStage;
    barrierInfo.srcAccessMask = mCurrentAccess;
    barrierInfo.dstAccessMask = nextAccess;
    barrierInfo.buffer = mBuffer;
    barrierInfo.offset = 0;
    barrierInfo.size = VK_WHOLE_SIZE;

    vk::DependencyInfo depInfo{};
    depInfo.pNext = nullptr;
    depInfo.pBufferMemoryBarriers = &barrierInfo;
    depInfo.bufferMemoryBarrierCount = 1;
    cmd.pipelineBarrier2(depInfo);

    mCurrentStage = nextStage;
    mCurrentAccess = nextAccess;
}

void SwBuffer::emitBarrier(vk::CommandBuffer cmd, SwDependency::BufferDepType bufferDepType) {
    emitBarrier(cmd, SwDependency::BufferDepDesc::get(bufferDepType).mStage, SwDependency::BufferDepDesc::get(bufferDepType).mAccess);
}

void SwBuffer::copyFrom(vk::CommandBuffer cmd, SwBuffer& src, vk::ArrayProxy<vk::BufferCopy> bufferCopies) {
    std::uint64_t biggestSize = 0;
    for (const auto& copy : bufferCopies) {
        biggestSize = std::max(biggestSize, copy.dstOffset + copy.size);
    }
    ensureCapacity(cmd, biggestSize);
    cmd.copyBuffer(*src.mBuffer, *mBuffer, bufferCopies);
    return;
}

void* SwBuffer::getMappedPtr() {
    if (mInfo.pMappedData == nullptr) {
        throw std::runtime_error("Buffer is not mapped");
    }
    return mInfo.pMappedData;
}

void SwBuffer::destroy() {
    if (mAllocation == nullptr) {
        return;
    }
    vk::Buffer rawBuffer = mBuffer.release();
    vmaDestroyBuffer(mAllocator, rawBuffer, mAllocation);
    mAllocator = nullptr;
    mAllocation = nullptr;
}

SwBuffer::SwBuffer(SwBuffer&& other) noexcept
    : mBuffer(std::move(other.mBuffer)),
      mAddress(other.mAddress),
      mAllocator(other.mAllocator),
      mAllocation(other.mAllocation),
      mInfo(other.mInfo),
      mFlags(other.mFlags),
      mUsage(other.mUsage),
      mSize(other.mSize),
      mGeneration(other.mGeneration),
      mCurrentStage(other.mCurrentStage),
      mCurrentAccess(other.mCurrentAccess) {
    other.mAllocator = nullptr;
    other.mAllocation = nullptr;
}

SwBuffer& SwBuffer::operator=(SwBuffer&& other) noexcept {
    if (this != &other) {
        destroy();

        mBuffer = std::move(other.mBuffer);
        mAddress = other.mAddress;
        mAllocator = other.mAllocator;
        mAllocation = other.mAllocation;
        mInfo = other.mInfo;
        mFlags = other.mFlags;
        mUsage = other.mUsage;
        mSize = other.mSize;
        mGeneration = other.mGeneration;
        mCurrentStage = other.mCurrentStage;
        mCurrentAccess = other.mCurrentAccess;

        other.mAllocator = nullptr;
        other.mAllocation = nullptr;
    }
    return *this;
}

SwBuffer::~SwBuffer() { destroy(); }

SwAllocatedBuffer::SwAllocatedBuffer() : SwBuffer() {}

SwAllocatedBuffer::SwAllocatedBuffer(
    vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info,
    vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint32_t size
)
    : SwBuffer(std::move(buffer), address, allocator, allocation, info, usage, flags, size) {}

void SwAllocatedBuffer::resize(vk::CommandBuffer cmd, std::uint32_t newSize) {
    vk::PipelineStageFlags2 prevStage = mCurrentStage;
    vk::AccessFlags2 prevAccess = mCurrentAccess;
    std::uint32_t prevGeneration = mGeneration;

    SwAllocatedBuffer newBuffer = SwBufferFactory::createAllocatedBuffer(mUsage, mFlags, newSize);

    if (mSize > 0) {
        emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
    }
    newBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);

    if (mSize > 0) {
        vk::BufferCopy copyRegion{};
        copyRegion.size = std::min(mSize, newSize);
        newBuffer.copyFrom(cmd, *this, copyRegion);
    }

    // Restore the new buffer to the state the old buffer was in so downstream barriers are correct.
    newBuffer.emitBarrier(cmd, prevStage, prevAccess);
    newBuffer.mGeneration = prevGeneration + 1;

    // Defer GPU-side destruction of the old handle; move new buffer into *this.
    SwBufferFactory::deferDestroy(std::make_unique<SwAllocatedBuffer>(std::move(*this)));
    static_cast<SwBuffer&>(*this) = std::move(newBuffer);
}

void SwAllocatedBuffer::ensureCapacity(vk::CommandBuffer cmd, std::uint32_t requiredSize) {
    if (requiredSize > mSize) {
        resize(cmd, std::max(requiredSize, mSize * 2));
    }
}

SwStagingBuffer::SwStagingBuffer() : SwBuffer() {}

SwStagingBuffer::SwStagingBuffer(vk::raii::Buffer buffer, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info, std::uint32_t size)
    : SwBuffer(
          std::move(buffer), std::nullopt, allocator, allocation, info, vk::BufferUsageFlagBits::eTransferSrc,
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, size
      ) {}

void SwStagingBuffer::resize(vk::CommandBuffer cmd, std::uint32_t newSize) {
    void* oldMapped = mInfo.pMappedData;
    std::uint32_t copySize = std::min(mSize, newSize);
    std::uint32_t prevGeneration = mGeneration;

    SwStagingBuffer newBuffer = SwBufferFactory::createStagingBuffer(newSize);

    if (oldMapped != nullptr && newBuffer.mInfo.pMappedData != nullptr && copySize > 0) {
        std::memcpy(newBuffer.mInfo.pMappedData, oldMapped, copySize);
    }

    newBuffer.mGeneration = prevGeneration + 1;

    SwBufferFactory::deferDestroy(std::make_unique<SwStagingBuffer>(std::move(*this)));
    static_cast<SwBuffer&>(*this) = std::move(newBuffer);
}

void SwStagingBuffer::ensureCapacity(vk::CommandBuffer cmd, std::uint32_t requiredSize) {
    if (requiredSize > mSize) {
        resize(cmd, std::max(requiredSize, mSize * 2));
    }
}

SwRendererContext SwBufferFactory::sRendererContext{};
std::vector<SwBufferFactory::DeferredBuffer> SwBufferFactory::sDeletionQueue{};

void SwBufferFactory::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

void SwBufferFactory::deferDestroy(std::unique_ptr<SwBuffer> buffer) {
    std::uint64_t currentFrame = sRendererContext.mSwapchain->getFrameNumber();
    sDeletionQueue.push_back({std::move(buffer), currentFrame});
}

void SwBufferFactory::tick(std::uint64_t currentFrame) {
    auto it = std::remove_if(sDeletionQueue.begin(), sDeletionQueue.end(), [currentFrame](const DeferredBuffer& entry) {
        return currentFrame >= entry.mFrameQueued + SwSwapchain::NUM_FRAME_OVERLAP;
    });
    sDeletionQueue.erase(it, sDeletionQueue.end());
}

SwAllocatedBuffer SwBufferFactory::createAllocatedBuffer(vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint32_t size) {
    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    auto bufferInfo1 = static_cast<VkBufferCreateInfo>(bufferInfo);

    VmaAllocationCreateInfo vmaCreateInfo = {};
    vmaCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateInfo.flags = flags | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer tempBuffer;
    VmaAllocation tempAllocation;
    VmaAllocationInfo tempInfo;
    vmaCreateBuffer(sRendererContext.mAllocator, &bufferInfo1, &vmaCreateInfo, &tempBuffer, &tempAllocation, &tempInfo);

    std::optional<vk::DeviceAddress> tempAddress = std::nullopt;
    if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        vk::BufferDeviceAddressInfo bufferDeviceAddressInfo = {};
        bufferDeviceAddressInfo.buffer = tempBuffer;
        tempAddress = sRendererContext.mDevice->getBufferAddress(bufferDeviceAddressInfo);
    }

    return SwAllocatedBuffer(
        vk::raii::Buffer(*sRendererContext.mDevice, tempBuffer),
        tempAddress,
        sRendererContext.mAllocator,
        tempAllocation,
        tempInfo,
        usage,
        flags | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        size
    );
}

SwStagingBuffer SwBufferFactory::createStagingBuffer(std::uint32_t size) {
    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = size;
    bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    auto bufferInfo1 = static_cast<VkBufferCreateInfo>(bufferInfo);

    VmaAllocationCreateInfo vmaCreateInfo = {};
    vmaCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer tempBuffer;
    VmaAllocation tempAllocation;
    VmaAllocationInfo tempInfo;
    vmaCreateBuffer(sRendererContext.mAllocator, &bufferInfo1, &vmaCreateInfo, &tempBuffer, &tempAllocation, &tempInfo);

    return SwStagingBuffer(vk::raii::Buffer(*sRendererContext.mDevice, tempBuffer), sRendererContext.mAllocator, tempAllocation, tempInfo, size);
}
