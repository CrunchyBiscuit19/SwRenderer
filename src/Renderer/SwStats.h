#pragma once

#include <Resource/SwBuffer.h>

class SwStats {
public:
    float mFrameTime;
    std::uint32_t mNumDrawCall;
    std::uint32_t mNumInitialRis;
    SwAllocatedBuffer mRisScratchCount;
    SwAllocatedBuffer mRisPublishedCount;

    SwStats();

    void initialize();

    void perFrameReset();
};