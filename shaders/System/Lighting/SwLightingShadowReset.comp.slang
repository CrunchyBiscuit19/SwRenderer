import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ResetPC shadowsResetPc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    // Get thread ID as index into render command
    uint rcsIndex = in.mDispatchThreadID.x;
    if (rcsIndex >= shadowsResetPc.mShadowsRcsLimit)
        return;

    // Reset render command instance count to zero
    shadowsResetPc.mShadowsRcsBuffer[rcsIndex].mRiCount = 0;
}
