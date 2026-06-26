#include <Renderer/SwRenderer.h>
#include <Resource/SwBuffer.h>

#include <algorithm>
#include <cstring>
#include <iostream>
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
    std::string name, vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info,
    vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint64_t size
)
    : mName(std::move(name)),
      mBuffer(std::move(buffer)),
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

void SwBuffer::ensureCapacity(vk::CommandBuffer cmd, std::uint64_t requiredSize) {
    if (requiredSize > mSize) {
        resize(cmd, std::max(requiredSize, mSize * 2));
    }
}

void SwBuffer::copyFrom(vk::CommandBuffer cmd, SwBuffer& src, vk::ArrayProxy<vk::BufferCopy> bufferCopies) {
    std::uint64_t biggestSize = 0;
    for (const auto& copy : bufferCopies) {
        biggestSize = std::max(biggestSize, copy.dstOffset + copy.size);
    }
    ensureCapacity(cmd, biggestSize);
    src.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
    emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);
    cmd.copyBuffer(*src.mBuffer, *mBuffer, bufferCopies);
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
    : mName(std::move(other.mName)),
      mBuffer(std::move(other.mBuffer)),
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

        mName = std::move(other.mName);
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
    std::string name, vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info,
    vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint64_t size
)
    : SwBuffer(std::move(name), std::move(buffer), address, allocator, allocation, info, usage, flags, size) {}

void SwAllocatedBuffer::resize(vk::CommandBuffer cmd, std::uint64_t newSize) {
    vk::PipelineStageFlags2 prevStage = mCurrentStage;
    vk::AccessFlags2 prevAccess = mCurrentAccess;
    std::uint32_t prevGeneration = mGeneration;

    SwAllocatedBuffer newBuffer = SwBufferFactory::createAllocatedBuffer(mName, mUsage, mFlags, newSize, mAddress.has_value());

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

void SwAllocatedBuffer::copyFrom(vk::CommandBuffer cmd, const void* src, std::uint64_t size, std::uint64_t internalOffset) {
    if (!(mFlags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT))) {
        throw std::runtime_error("copyFrom(ptr) requires host-accessible allocation");
    }
    ensureCapacity(cmd, internalOffset + size);
    std::memcpy(static_cast<char*>(mInfo.pMappedData) + internalOffset, src, size);
    emitBarrier(cmd, vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostWrite);
}

void SwAllocatedBuffer::copyFromUnchecked(const void* src, std::uint64_t size, std::uint64_t internalOffset) {
    if (!(mFlags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT))) {
        throw std::runtime_error("copyFromUnchecked requires host-accessible allocation");
    }
    std::memcpy(static_cast<char*>(mInfo.pMappedData) + internalOffset, src, size);
    mCurrentStage = vk::PipelineStageFlagBits2::eHost;
    mCurrentAccess = vk::AccessFlagBits2::eHostWrite;
}

SwStagingBuffer::SwStagingBuffer() : SwBuffer() {}

SwStagingBuffer::SwStagingBuffer(std::string name, vk::raii::Buffer buffer, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info, std::uint64_t size)
    : SwBuffer(
          std::move(name), std::move(buffer), std::nullopt, allocator, allocation, info, vk::BufferUsageFlagBits::eTransferSrc,
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, size
      ) {}

void SwStagingBuffer::resize(vk::CommandBuffer cmd, std::uint64_t newSize) {
    void* oldMapped = mInfo.pMappedData;
    std::uint64_t copySize = std::min(mSize, newSize);
    vk::PipelineStageFlags2 prevStage = mCurrentStage;
    vk::AccessFlags2 prevAccess = mCurrentAccess;
    std::uint32_t prevGeneration = mGeneration;

    SwStagingBuffer newBuffer = SwBufferFactory::createStagingBuffer(mName, newSize);

    if (oldMapped != nullptr && newBuffer.mInfo.pMappedData != nullptr && copySize > 0) {
        std::memcpy(newBuffer.mInfo.pMappedData, oldMapped, copySize);
    }

    // Record the host write so the transition to prevStage emits srcStage=eHost/srcAccess=eHostWrite,
    // making the memcpy visible to subsequent GPU reads.
    newBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostWrite);
    newBuffer.emitBarrier(cmd, prevStage, prevAccess);
    newBuffer.mGeneration = prevGeneration + 1;

    SwBufferFactory::deferDestroy(std::make_unique<SwStagingBuffer>(std::move(*this)));
    static_cast<SwBuffer&>(*this) = std::move(newBuffer);
}

void SwStagingBuffer::copyFrom(vk::CommandBuffer cmd, const void* src, std::uint64_t size, std::uint64_t internalOffset) {
    ensureCapacity(cmd, internalOffset + size);
    std::memcpy(static_cast<char*>(mInfo.pMappedData) + internalOffset, src, size);
}

void SwStagingBuffer::copyFromUnchecked(const void* src, std::uint64_t size, std::uint64_t internalOffset) {
    std::memcpy(static_cast<char*>(mInfo.pMappedData) + internalOffset, src, size);
    mCurrentStage = vk::PipelineStageFlagBits2::eHost;
    mCurrentAccess = vk::AccessFlagBits2::eHostWrite;
}





std::vector<SwBufferFactory::DeferredBuffer> SwBufferFactory::sDeletionQueue{};


void SwBufferFactory::deferDestroy(std::unique_ptr<SwBuffer> buffer) {
    std::uint64_t currentFrame = SwRenderer::sRendererContext.mSwapchain->getFrameNumber();
    sDeletionQueue.emplace_back(std::move(buffer), currentFrame);
}

void SwBufferFactory::tick(std::uint64_t currentFrame) {
    auto it = std::remove_if(sDeletionQueue.begin(), sDeletionQueue.end(), [currentFrame](const DeferredBuffer& entry) {
        return currentFrame >= entry.mFrameQueued + SwSwapchain::NUM_FRAME_OVERLAP;
    });
    sDeletionQueue.erase(it, sDeletionQueue.end());
}

SwAllocatedBuffer SwBufferFactory::createAllocatedBuffer(
    std::string name, vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint64_t size, bool addressable, bool resizable
) {
    if (addressable) usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
    if (resizable) usage |= vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
    flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    auto bufferInfo1 = static_cast<VkBufferCreateInfo>(bufferInfo);

    VmaAllocationCreateInfo vmaCreateInfo = {};
    vmaCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateInfo.flags = flags;

    VkBuffer tempBuffer;
    VmaAllocation tempAllocation;
    VmaAllocationInfo tempInfo;
    vmaCreateBuffer(SwRenderer::sRendererContext.mAllocator, &bufferInfo1, &vmaCreateInfo, &tempBuffer, &tempAllocation, &tempInfo);

    std::optional<vk::DeviceAddress> tempAddress = std::nullopt;
    if (addressable || usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        vk::BufferDeviceAddressInfo bufferDeviceAddressInfo = {};
        bufferDeviceAddressInfo.buffer = tempBuffer;
        tempAddress = SwRenderer::sRendererContext.mDevice->getBufferAddress(bufferDeviceAddressInfo);
    }

    SwAllocatedBuffer buffer(
        std::move(name), vk::raii::Buffer(*SwRenderer::sRendererContext.mDevice, tempBuffer), tempAddress, SwRenderer::sRendererContext.mAllocator, tempAllocation,
        tempInfo, usage, flags, size
    );
    SwRenderer::sRendererContext.labelResourceDebug(buffer.getHandle(), buffer.getName().c_str());
    return buffer;
}

SwStagingBuffer SwBufferFactory::createStagingBuffer(std::string name, std::uint64_t size, bool resizable) {
    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = size;
    bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    if (resizable) bufferInfo.usage |= vk::BufferUsageFlagBits::eTransferDst;
    auto bufferInfo1 = static_cast<VkBufferCreateInfo>(bufferInfo);

    VmaAllocationCreateInfo vmaCreateInfo = {};
    vmaCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer tempBuffer;
    VmaAllocation tempAllocation;
    VmaAllocationInfo tempInfo;
    vmaCreateBuffer(SwRenderer::sRendererContext.mAllocator, &bufferInfo1, &vmaCreateInfo, &tempBuffer, &tempAllocation, &tempInfo);

    SwStagingBuffer buffer(
        std::move(name), vk::raii::Buffer(*SwRenderer::sRendererContext.mDevice, tempBuffer), SwRenderer::sRendererContext.mAllocator, tempAllocation, tempInfo, size
    );
    SwRenderer::sRendererContext.labelResourceDebug(buffer.getHandle(), buffer.getName().c_str());
    return buffer;
}
