import SwCull;
import SwCommon;

[[vk::push_constant]]
SwCull::CompactPC compactPc;

void copyRenderItem(SwRenderItem* src, SwRenderItem* dst) {
    dst.mIndexCount = src.mIndexCount;
    dst.mRInstCount = src.mRInstCount;
    dst.mFirstIndex = src.mFirstIndex;
    dst.mVertexOffset = src.mVertexOffset;
    dst.mFirstRInst = src.mFirstRInst;
    dst.mMaterialIndex = src.mMaterialIndex;
    dst.mNodeTransformIndex = src.mNodeTransformIndex;
    dst.mModelIndex = src.mModelIndex;
    dst.mFirstInstance = src.mFirstInstance;
    dst.mBoundsIndex = src.mBoundsIndex;
}

[numthreads(SwCull::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    // Get thread ID as index into render item
    uint renderItemsIndex = in.mDispatchThreadID.x;
    if (renderItemsIndex >= compactPc.mPreRItemsLimit)
        return;

    // Copy over render items with more than zero instances, and increment counter
    SwRenderItem* rItem = &compactPc.mPreRItemsBuffer[renderItemsIndex];

    if (rItem->mRInstCount == 0)
        return;

    uint writeRenderItemsIndex;
    InterlockedAdd(*compactPc.mPostRItemsCount, 1, writeRenderItemsIndex);
    copyRenderItem(rItem, &compactPc.mPostRItemsBuffer[writeRenderItemsIndex]);
}
