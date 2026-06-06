import SwCull;
import SwCommon;

[[vk::push_constant]]
SwCull::CompactPC compactPc;

[numthreads(SwCull::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    // Get thread ID as index into render item
    uint rItemsIndex = in.mDispatchThreadID.x;
    if (rItemsIndex >= compactPc.mPreRItemsLimit)
        return;

    // Copy over render items with more than zero instances, and increment counter
    SwRenderItem* rItem = &compactPc.mPreRItemsBuffer[rItemsIndex];

    if (rItem->mRInstCount == 0)
        return;

    uint writeRItemsIndex;
    InterlockedAdd(*compactPc.mPostRItemsCount, 1, writeRItemsIndex);
    compactPc.mPostRItemsBuffer[writeRItemsIndex].copyFrom(rItem);
}
