#include <Resources/SwBuffer.h>
#include <Renderer/SwRenderer.h>

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

void SwBuffer::barrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess) {
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

void SwBuffer::copyFrom(vk::CommandBuffer cmd, SwBuffer& src, vk::ArrayProxy<vk::BufferCopy> bufferCopies, std::uint32_t maxSize) {
    if (maxSize > mSize) {
        throw std::invalid_argument("Copy size too big");
    }
    cmd.copyBuffer(*src.mBuffer, *mBuffer, bufferCopies);
    return;
}

void* SwBuffer::getMappedPointer() {
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

SwStagingBuffer::SwStagingBuffer() : SwBuffer() {}

SwStagingBuffer::SwStagingBuffer(vk::raii::Buffer buffer, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info, std::uint32_t size)
    : SwBuffer(
          std::move(buffer), std::nullopt, allocator, allocation, info, vk::BufferUsageFlagBits::eTransferSrc,
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, size
      ) {}

SwRendererContext SwBufferFactory::sRendererContext{};

void SwBufferFactory::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

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
