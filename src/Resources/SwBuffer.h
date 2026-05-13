#pragma once

#include <vk_mem_alloc.h>

#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwBuffer {
protected:
    std::optional<vk::DeviceAddress> mAddress;
    VmaAllocationInfo mInfo;
    VmaAllocationCreateFlags mFlags;
    vk::BufferUsageFlags mUsage;
    std::uint32_t mSize;
    vk::PipelineStageFlags2 mCurrentStage;
    vk::AccessFlags2 mCurrentAccess;
    VmaAllocator mAllocator;
    VmaAllocation mAllocation;
    vk::raii::Buffer mBuffer;

    SwBuffer();

    SwBuffer(
        vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info,
        vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint32_t size
    );

public:
    void barrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess);

    void copyFrom(vk::CommandBuffer cmd, SwBuffer& src, vk::ArrayProxy<vk::BufferCopy> bufferCopies, std::uint32_t maxSize);

    void* getMappedPointer();

    inline vk::Buffer getRawBuffer() { return *mBuffer; }

    void destroy();

    SwBuffer(SwBuffer&&) noexcept;
    SwBuffer& operator=(SwBuffer&&) noexcept;

    SwBuffer(const SwBuffer&) = delete;
    SwBuffer& operator=(const SwBuffer&) = delete;

    ~SwBuffer();
};

class SwAllocatedBuffer : public SwBuffer {
public:
    SwAllocatedBuffer();

    SwAllocatedBuffer(
        vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info,
        vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint32_t size
    );

    SwAllocatedBuffer(SwAllocatedBuffer&&) noexcept = default;
    SwAllocatedBuffer& operator=(SwAllocatedBuffer&&) noexcept = default;

    SwAllocatedBuffer(const SwAllocatedBuffer&) = delete;
    SwAllocatedBuffer& operator=(const SwAllocatedBuffer&) = delete;
};

class SwStagingBuffer : public SwBuffer {
public:
    SwStagingBuffer();

    SwStagingBuffer(vk::raii::Buffer buffer, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info, std::uint32_t size);

    SwStagingBuffer(SwStagingBuffer&&) noexcept = default;
    SwStagingBuffer& operator=(SwStagingBuffer&&) noexcept = default;

    SwStagingBuffer(const SwStagingBuffer&) = delete;
    SwStagingBuffer& operator=(const SwStagingBuffer&) = delete;
};

class SwBufferFactory {
private:
    enum class SwBufferType { SwAddressedBuffer, SwStagingBuffer };

    static SwRendererContext sRendererContext;

public:
    static void init(SwRendererContext rendererContext);

    static SwAllocatedBuffer createAllocatedBuffer(vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint32_t size);

    static SwStagingBuffer createStagingBuffer(std::uint32_t size);
};