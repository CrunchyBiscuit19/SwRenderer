import SwCull;

[[vk::push_constant]]
SwCull::ResetPC resetPc;

[numthreads(SwCull::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    // Get thread ID as index into render item
    uint renderItemsIndex = in.mDispatchThreadID.x;
    if (renderItemsIndex >= resetPc.mPreCullRenderItemsLimit) return;

    // Reset render item instance count to zero
    resetPc.mPreCullRenderItemsBuffer[renderItemsIndex].mRenderInstanceCount = 0;
}