#pragma once

#include <Resource/SwBuffer.h>

#include <cstdint>
#include <deque>

// Large ring staging buffer
class SwStagingRing {
private:
    struct Region {
        std::uint64_t mEnd;  
        std::uint64_t mFrame;    
    };

    SwStagingBuffer mRing;
    std::uint64_t mCapacity{0};
    std::uint64_t mHead{0};   
    std::uint64_t mTail{0};       
    std::uint64_t mAlignment{1}; 
    std::deque<Region> mInFlight;

    void grow(std::uint64_t requiredSize);

public:
    static constexpr std::uint64_t INITIAL_CAPACITY{1 << 4};

    static std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment) { return (value + alignment - 1) / alignment * alignment; };

    void initialize(std::uint64_t capacity);
    void destroy();

    // Copies size bytes from src into a fresh ring region, then records a ring-to-dst copy on cmd.
    void upload(vk::CommandBuffer cmd, SwBuffer& dst, const void* src, std::uint64_t size, std::uint64_t dstOffset = 0);

    // Reclaims every region whose frame window has elapsed. Call once per frame near SwBufferFactory::tick.
    void tick(std::uint64_t currentFrame);
};
