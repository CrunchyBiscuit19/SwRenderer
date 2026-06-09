import SwCull;

[[vk::push_constant]]
SwCull::ResetPC resetPc;

[numthreads(Sw::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    // Get thread ID as index into render command
    uint rcsIndex = in.mDispatchThreadID.x;
    if (rcsIndex >= resetPc.mRcsLimit)
        return;

    // Reset render command instance count to zero
    resetPc.mRcsBuffer[rcsIndex].mRiCount = 0;
}
