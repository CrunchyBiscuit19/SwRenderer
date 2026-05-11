import Culler;
import RenderItem;

static const uint MAX_1D_WORKGROUP_THREADS = 1024;

[[vk::push_constant]]
CullerCompactPushConstants cullerCompactPc;

void copyRenderItem(RenderItem *src, RenderItem *dst) {
    dst.indexCount = src.indexCount;
    dst.renderInstanceCount = src.renderInstanceCount;
    dst.firstIndex = src.firstIndex;
    dst.vertexOffset = src.vertexOffset;
    dst.firstRenderInstance = src.firstRenderInstance;
    dst.materialIndex = src.materialIndex;
    dst.nodeTransformIndex = src.nodeTransformIndex;
    dst.modelIndex = src.modelIndex;
    dst.firstInstance = src.firstInstance;
    dst.boundsIndex = src.boundsIndex;
}

[numthreads(MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(CSInput in) {
    // Get thread ID as index into render item
    uint renderItemsIndex = in.dispatchThreadID.x;
    if (renderItemsIndex >= cullerCompactPc.preCullRenderItemsLimit) return;

    // Copy over render items with more than zero instances, and increment counter
    RenderItem* renderItem = &cullerCompactPc.preCullRenderItemsBuffer[renderItemsIndex];
    
    if (renderItem->renderInstanceCount == 0) return;
    
    uint writeRenderItemsIndex;
    InterlockedAdd(*cullerCompactPc.postCullRenderItemsCountBuffer, 1, writeRenderItemsIndex);
    copyRenderItem(renderItem, &cullerCompactPc.postCullRenderItemsBuffer[writeRenderItemsIndex]);
}