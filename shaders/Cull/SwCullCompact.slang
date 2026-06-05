import SwCull;
import SwCommon;

[[vk::push_constant]]
SwCull::CompactPC compactPc;

void copyRenderItem(SwRenderItem* src, SwRenderItem* dst) {
    dst.mIndexCount = src.mIndexCount;
    dst.mRenderInstanceCount = src.mRenderInstanceCount;
    dst.mFirstIndex = src.mFirstIndex;
    dst.mVertexOffset = src.mVertexOffset;
    dst.mFirstRenderInstance = src.mFirstRenderInstance;
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
    if (renderItemsIndex >= compactPc.mPreRenderItemsLimit)
        return;

    // Copy over render items with more than zero instances, and increment counter
    SwRenderItem* renderItem = &compactPc.mPreRenderItemsBuffer[renderItemsIndex];

    if (renderItem->mRenderInstanceCount == 0)
        return;

    uint writeRenderItemsIndex;
    InterlockedAdd(*compactPc.mPostRenderItemsCountBuffer, 1, writeRenderItemsIndex);
    copyRenderItem(renderItem, &compactPc.mPostRenderItemsBuffer[writeRenderItemsIndex]);
}
