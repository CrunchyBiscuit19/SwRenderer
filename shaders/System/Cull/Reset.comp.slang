import System.Cull.SwCull;
import Data.SwConstants;

[[vk::push_constant]]
SwCull::ResetPC resetPc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    uint rcsIndex = in.mDispatchThreadID.x;
    if (rcsIndex >= resetPc.mSceneRcsLimit)
        return;

    resetPc.mSceneRcsBuffer[rcsIndex].mRiCount = 0;
}