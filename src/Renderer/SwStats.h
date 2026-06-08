# pragma once

#include <Resource/SwBuffer.h>

class SwStats {
public:
    float mFrameTime;
    float mDrawTime;
    std::uint32_t mNumDrawCall;
    std::uint32_t mNumInitialRis;
    SwAllocatedBuffer mRisScratchCount;
    SwAllocatedBuffer mRisPublishedCount;
    float mSceneUpdateTime;

    SwStats();

    void initialize();

    void perFrameReset();
};