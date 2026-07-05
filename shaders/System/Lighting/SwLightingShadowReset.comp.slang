import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ShadowResetPC shadowResetPc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    // Get thread ID as index into render command
    uint rcsIndex = in.mDispatchThreadID.x;
    if (rcsIndex >= shadowResetPc.mShadowRcsLimit)
        return;

    // Reset render command instance count to zero
    shadowResetPc.mShadowDrawRcsBuffer[rcsIndex].mRiCount = 0;
}
