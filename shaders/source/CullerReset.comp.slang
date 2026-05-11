import Culler;
import RenderItem;

static const uint MAX_1D_WORKGROUP_THREADS = 1024;

[[vk::push_constant]]
CullerResetPushConstants cullerResetPc;

[numthreads(MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(CSInput in) {
    // Get thread ID as index into render item
    uint renderItemsIndex = in.dispatchThreadID.x;
    if (renderItemsIndex >= cullerResetPc.preCullRenderItemsLimit) return;

    // Reset render item instance count to zero
    cullerResetPc.preCullRenderItemsBuffer[renderItemsIndex].renderInstanceCount = 0;
}