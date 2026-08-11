import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ShadowsCompactPC shadowsCompactPc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint rcsIndex = in.mDispatchThreadID.x;
    if (rcsIndex >= shadowsCompactPc.mShadowsPreRcsLimit)
        return;

    SwRenderCommand* rc = &shadowsCompactPc.mShadowsPreRcsBuffer[rcsIndex];
    if (rc->mRiCount == 0)
        return;

    uint numOpaqueRcs = shadowsCompactPc.mNumOpaqueRcsPerShadowView;
    uint numMaskRcs = shadowsCompactPc.mNumMaskRcsPerShadowView;
    uint numViewRcs = numOpaqueRcs + numMaskRcs;

    // Each light type owns a contiguous region of opaque rcs followed by mask rcs, giving the six segments the counters index.
    uint dirOpaqueBase = 0;
    uint dirMaskBase = dirOpaqueBase + numOpaqueRcs * SwLighting::Shadow::NUM_DIRECTIONAL_VIEWS;
    uint pointOpaqueBase = numViewRcs * SwLighting::Shadow::POINT_VIEW_BASE;
    uint pointMaskBase = pointOpaqueBase + numOpaqueRcs * SwLighting::Shadow::NUM_POINT_VIEWS;
    uint spotOpaqueBase = numViewRcs * SwLighting::Shadow::SPOT_VIEW_BASE;
    uint spotMaskBase = spotOpaqueBase + numOpaqueRcs * SwLighting::Shadow::NUM_SPOT_VIEWS;

    uint segment;
    uint segmentBase;
    uint segmentViewBase;
    uint segmentPerViewRcs;
    if (rcsIndex < dirMaskBase) {
        segment = 0;
        segmentBase = dirOpaqueBase;
        segmentViewBase = SwLighting::Shadow::DIRECTIONAL_VIEW_BASE;
        segmentPerViewRcs = numOpaqueRcs;
    } else if (rcsIndex < pointOpaqueBase) {
        segment = 1;
        segmentBase = dirMaskBase;
        segmentViewBase = SwLighting::Shadow::DIRECTIONAL_VIEW_BASE;
        segmentPerViewRcs = numMaskRcs;
    } else if (rcsIndex < pointMaskBase) {
        segment = 2;
        segmentBase = pointOpaqueBase;
        segmentViewBase = SwLighting::Shadow::POINT_VIEW_BASE;
        segmentPerViewRcs = numOpaqueRcs;
    } else if (rcsIndex < spotOpaqueBase) {
        segment = 3;
        segmentBase = pointMaskBase;
        segmentViewBase = SwLighting::Shadow::POINT_VIEW_BASE;
        segmentPerViewRcs = numMaskRcs;
    } else if (rcsIndex < spotMaskBase) {
        segment = 4;
        segmentBase = spotOpaqueBase;
        segmentViewBase = SwLighting::Shadow::SPOT_VIEW_BASE;
        segmentPerViewRcs = numOpaqueRcs;
    } else {
        segment = 5;
        segmentBase = spotMaskBase;
        segmentViewBase = SwLighting::Shadow::SPOT_VIEW_BASE;
        segmentPerViewRcs = numMaskRcs;
    }

    uint absoluteView = segmentViewBase + (rcsIndex - segmentBase) / segmentPerViewRcs;

    uint segmentOffset;
    InterlockedAdd(shadowsCompactPc.mShadowsRcsCount[segment], 1, segmentOffset);
    SwRenderCommand* dst = &shadowsCompactPc.mShadowsPostRcsBuffer[segmentBase + segmentOffset];
    dst->copyFrom(rc);
    dst->mShadowView = absoluteView;
}
