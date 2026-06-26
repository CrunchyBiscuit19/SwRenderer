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
    std::string mName;
    std::optional<vk::DeviceAddress> mAddress;
    VmaAllocationInfo mInfo;
    VmaAllocationCreateFlags mFlags;
    vk::BufferUsageFlags mUsage;
    std::uint64_t mSize;
    std::uint32_t mGeneration{0}; 
    vk::PipelineStageFlags2 mCurrentStage;
    vk::AccessFlags2 mCurrentAccess;
    VmaAllocator mAllocator;
    VmaAllocation mAllocation;
    vk::raii::Buffer mBuffer;

    SwBuffer();

    SwBuffer(
        std::string name, vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info,
        vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint64_t size
    );

    // Allocates new bigger buffer, copies old content, restores pipeline stage/access on the new buffer 
    // Defers destruction of the old VkBuffer until no longer in-flight. 
    // Descriptor sets referencing this buffer must be rewritten wheb getGeneration() changes.
    virtual void resize(vk::CommandBuffer cmd, std::uint64_t newSize) = 0;

public:
    void emitBarrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess);
    void emitBarrier(vk::CommandBuffer cmd, SwDependency::BufferDepType bufferDepType);

    void ensureCapacity(vk::CommandBuffer cmd, std::uint64_t requiredSize);

    void copyFrom(vk::CommandBuffer cmd, SwBuffer& src, vk::ArrayProxy<vk::BufferCopy> bufferCopies);
    virtual void copyFrom(vk::CommandBuffer cmd, const void* src, std::uint64_t size, std::uint64_t internalOffset = 0) = 0;

    virtual void copyFromUnchecked(const void* src, std::uint64_t size, std::uint64_t internalOffset = 0) = 0;

    void* getMappedPtr();
    inline const std::string& getName() const { return mName; }
    inline vk::Buffer getHandle() { return *mBuffer; }
    std::optional<vk::DeviceAddress> getDeviceAddress() { return mAddress; }
    inline vk::PipelineStageFlags2 getCurrentStage() { return mCurrentStage; }
    inline vk::AccessFlags2 getCurrentAccess() { return mCurrentAccess; }
    inline std::uint64_t getSize() const { return mSize; }
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
    void resize(vk::CommandBuffer cmd, std::uint64_t newSize) override;

public:
    SwAllocatedBuffer();

    SwAllocatedBuffer(
        std::string name, vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info,
        vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint64_t size
    );

    using SwBuffer::copyFrom;
    void copyFrom(vk::CommandBuffer cmd, const void* src, std::uint64_t size, std::uint64_t internalOffset = 0) override;

    virtual void copyFromUnchecked(const void* src, std::uint64_t size, std::uint64_t internalOffset = 0) override;

    SwAllocatedBuffer(SwAllocatedBuffer&&) noexcept = default;
    SwAllocatedBuffer& operator=(SwAllocatedBuffer&&) noexcept = default;

    SwAllocatedBuffer(const SwAllocatedBuffer&) = delete;
    SwAllocatedBuffer& operator=(const SwAllocatedBuffer&) = delete;
};

class SwStagingBuffer : public SwBuffer {
private:
    void resize(vk::CommandBuffer cmd, std::uint64_t newSize) override;

public:
    SwStagingBuffer();

    SwStagingBuffer(std::string name, vk::raii::Buffer buffer, VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo info, std::uint64_t size);

    using SwBuffer::copyFrom;
    void copyFrom(vk::CommandBuffer cmd, const void* src, std::uint64_t size, std::uint64_t internalOffset = 0) override;

    virtual void copyFromUnchecked(const void* src, std::uint64_t size, std::uint64_t internalOffset = 0) override;

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

    static std::vector<DeferredBuffer> sDeletionQueue;

public:
    static void init();

    static SwAllocatedBuffer createAllocatedBuffer(
        std::string name, vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags, std::uint64_t size, bool addressable = false, bool resizable = true
    );

    static SwStagingBuffer createStagingBuffer(std::string name, std::uint64_t size, bool resizable = true);

    // Queues a buffer for destruction after NUM_FRAME_OVERLAP frames have passed.
    // Call this instead of letting resize destroy the old handle immediately.
    static void deferDestroy(std::unique_ptr<SwBuffer> buffer);

    // Destroys all queued buffers whose frame window has elapsed. Call once per frame.
    static void tick(std::uint64_t currentFrame);
};
