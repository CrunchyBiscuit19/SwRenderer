#pragma once

#include <Resource/SwBuffer.h>

class SwStats {
public:
    float mFrameTime{0.f};
    std::uint32_t mNumDrawCall{0};
    std::uint32_t mNumInitialRis{0};
    SwAllocatedBuffer mRisScratchCount;
    SwAllocatedBuffer mRisPublishedCount;

    SwStats();

    void initialize();

    void perFrameReset();
};