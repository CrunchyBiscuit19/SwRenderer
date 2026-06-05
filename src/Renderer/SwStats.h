# pragma once

#include <Resource/SwBuffer.h>

class SwStats {
public:
    float mFrameTime;
    float mDrawTime;
    std::uint32_t mNumDrawCall;
    std::uint32_t mNumInitialRInsts;
    SwAllocatedBuffer mRInstsCount;
    float mSceneUpdateTime;

    SwStats();

    void initialize();

    void perFrameReset();
};