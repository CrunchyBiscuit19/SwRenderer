#pragma once

#include <Resource/SwBuffer.h>

#include <cstdint>
#include <deque>

// Large ring staging buffer
// Each region is tagged with the frame it was recorded in and reclaimed once that frame retires.
// On resize, new buffer is created and the old one is deferred-destroyed so every already recorded command can still reference it.
class SwStagingRing {
private:
    struct Region {
        std::uint64_t mHeadEnd;  // Monotonic head position immediately after this region.
        std::uint64_t mFrame;    // Frame number the region was recorded in.
    };

    SwStagingBuffer mRing;
    std::uint64_t mCapacity{0};
    std::uint64_t mHead{0};       
    std::uint64_t mTail{0};       
    std::uint64_t mAlignment{1};  // Sub-allocation alignment derived from device copy limits.
    std::deque<Region> mInFlight;

    void grow(std::uint64_t requiredSize);

public:
    static constexpr std::uint64_t INITIAL_CAPACITY{1 << 4};

    static std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment) { return (value + alignment - 1) / alignment * alignment; };

    void initialize(std::uint64_t capacity);
    void destroy();

    // Copies size bytes from src into a fresh ring region, then records a ring-to-dst copy on cmd.
    // The region is tagged with the current frame and reclaimed automatically once that frame retires.
    void upload(vk::CommandBuffer cmd, SwBuffer& dst, const void* src, std::uint64_t size, std::uint64_t dstOffset = 0);

    // Reclaims every region whose frame window has elapsed. Call once per frame near SwBufferFactory::tick.
    void tick(std::uint64_t currentFrame);
};
