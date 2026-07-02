import SwCull;
import SwCommon;

[[vk::push_constant]]
SwCull::CompactPC compactPc;

[numthreads(Sw::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    // Get thread ID as index into render command
    uint rcsIndex = in.mDispatchThreadID.x;
    if (rcsIndex >= compactPc.mPreRcsLimit)
        return;

    // Copy over render commands with more than zero instances, and increment counter
    Sw::RenderCommand* rc = &compactPc.mPreRcsBuffer[rcsIndex];

    if (rc->mRiCount == 0)
        return;

    uint writeRcsIndex;
    InterlockedAdd(*compactPc.mPostRcsCount, 1, writeRcsIndex);
    compactPc.mPostRcsBuffer[writeRcsIndex].copyFrom(rc);
}
