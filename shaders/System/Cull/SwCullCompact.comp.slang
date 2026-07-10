import System.Cull.SwCull;
import Data.SwConstants;

[[vk::push_constant]]
SwCull::CompactPC compactPc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    uint rcsIndex = in.mDispatchThreadID.x;
    if (rcsIndex >= compactPc.mPreRcsLimit)
        return;

    // Copy over render commands with more than zero instances, and increment counter
    SwBatch::RenderCommand* rc = &compactPc.mPreRcsBuffer[rcsIndex];

    if (rc->mRiCount == 0)
        return;

    // Current batch's first rcs slot in the scene wide pre-rcs buffer
    uint currentBatchStartRcsIndex = compactPc.mSceneBatchesBuffer[rc->mBatchIndex].mRcsIndex;
    
    // Current batch's rcs count, incremented for next thread. 
    uint currentBatchCurrentRcsCount;
    InterlockedAdd(compactPc.mPostRcsCount[rc->mBatchIndex], 1, currentBatchCurrentRcsCount);
    
    // Current rcs's index in the scene wide post-rcs buffer
    uint writeRcsIndex = currentBatchStartRcsIndex + currentBatchCurrentRcsCount;
    compactPc.mPostRcsBuffer[writeRcsIndex].copyFrom(rc);
}