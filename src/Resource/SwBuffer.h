#pragma once

#include <Scene/SwDependency.h>
#include <vk_mem_alloc.h>

#include <memory>
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
    std::uint32_t mGeneration{0};  // Incremented on every resize; callers use this to detect stale descriptor set bindings.
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

    // Allocates new bigger buffer, copies old content, restores pipeline stage/access on the new buffer 
    // Defers destruction of the old VkBuffer until no longer in-flight. 
    // Descriptor sets referencing this buffer must be rewritten wheb getGeneration() changes.
    virtual void resize(vk::CommandBuffer cmd, std::uint32_t newSize) = 0;

    virtual void ensureCapacity(vk::CommandBuffer cmd, std::uint32_t requiredSize) = 0;

public:
    void emitBarrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess);
    void emitBarrier(vk::CommandBuffer cmd, SwDependency::BufferDepType bufferDepType);

    void copyFrom(vk::CommandBuffer cmd, SwBuffer& src, vk::ArrayProxy<vk::BufferCopy> bufferCopies);

    void* getMappedPtr();
    inline vk::Buffer getRawBuffer() { return *mBuffer; }
    std::optional<vk::DeviceAddress> getDeviceAddress() { return mAddress; }
    inline vk::PipelineStageFlags2 getCurrentStage() { return mCurrentStage; }
    inline vk::AccessFlags2 getCurrentAccess() { return mCurrentAccess; }
    inline std::uint32_t getSize() const { return mSize; }
    inline std::uint32_t getGeneration() const { return mGeneration; }

    void destroy();

    SwBuffer(SwBuffer&&) noexcept;
    SwBuffer& operator=(SwBuffer&&) noexcept;

    SwBuffer(const SwBuffer&) = delete;
    SwBuffer& operator=(const SwBuffer&) = delete;

    virtual ~SwBuffer();
};

class SwAllocatedBuffer : public SwBuffer {
private:
    void resize(vk::CommandBuffer cmd, std::uint32_t newSize) override;

    void ensureCapacity(vk::CommandBuffer cmd, std::uint32_t requiredSize) override;

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
private:
    void resize(vk::CommandBuffer cmd, std::uint32_t newSize) override;

    void ensureCapacity(vk::CommandBuffer cmd, std::uint32_t requiredSize) override;

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

    struct DeferredBuffer {
        std::unique_ptr<SwBuffer> mBuffer;
        std::uint64_t mFrameQueued;
    };

    static SwRendererContext sRendererContext;
    static std::vector<DeferredBuffer> sDeletionQueue;

public:
    static void init(SwRendererContext rendererContext);

    static SwAllocatedBuffer createAllocatedBuffer(vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint32_t size);

    static SwStagingBuffer createStagingBuffer(std::uint32_t size);

    // Queues a buffer for destruction after NUM_FRAME_OVERLAP frames have passed.
    // Call this instead of letting resize destroy the old handle immediately.
    static void deferDestroy(std::unique_ptr<SwBuffer> buffer);

    // Destroys all queued buffers whose frame window has elapsed. Call once per frame.
    static void tick(std::uint64_t currentFrame);
};
