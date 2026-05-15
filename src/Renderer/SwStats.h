# pragma once

#include <Resource/SwBuffer.h>

class SwStats {
public:
    float mFrameTime;
    float mDrawTime;
    std::uint32_t mDrawCallCount;
    std::uint32_t mPreCullRenderInstancesCount;
    SwAllocatedBuffer mRenderInstancesCountBuffer;
    float mSceneUpdateTime;

    SwStats();

    void initialize();

    void reset();
};