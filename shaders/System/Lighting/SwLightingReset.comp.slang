import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ResetPC resetPc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint rcsIndex = in.mDispatchThreadID.x;
    if (rcsIndex >= resetPc.mSceneShadowsRcsLimit)
        return;

    resetPc.mSceneShadowsRcsBuffer[rcsIndex].mRiCount = 0;
}
