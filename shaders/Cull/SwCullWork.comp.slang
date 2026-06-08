import SwCull;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthPyramidImage;

[[vk::binding(1, 0)]]
public uniform SamplerState minSampler;

[[vk::push_constant]]
SwCull::WorkPC workPc;

void transformCornersToWorld(inout float4[SwCull::NUM_AABB_CORNERS] aabbCorners, SwInstance* currInstance, SwNodeTransform* nodeTransform) {
    for (uint j = 0; j < SwCull::NUM_AABB_CORNERS; j++) {
        aabbCorners[j] = mul(currInstance->mTransformMatrix, mul(*nodeTransform, aabbCorners[j]));
    }
}

bool frustumTest(SwCull::Plane* frustum, float4[SwCull::NUM_AABB_CORNERS] aabbCorners) {
    bool visible = true;

    // Get the min and max corners of the (already world-space) AABB
    float4 aabbMin = aabbCorners[0];
    float4 aabbMax = aabbCorners[0];
    for (uint j = 1; j < SwCull::NUM_AABB_CORNERS; j++) {
        aabbMin = min(aabbMin, aabbCorners[j]);
        aabbMax = max(aabbMax, aabbCorners[j]);
    }

    // For each plane build the corner which will produce max result in point-normal plane equation
    // Point-normal plane equation is positive if it's on the side of the plane the normal is pointing
    // Frustum plane normals point inward => non-negative result means at least 1 corner is on correct side of the plane
    // At least 1 corner on correct side of all 6 planes means render item is within frustum and therefore visible
    for (uint i = 0; i < SwCull::NUM_FRUSTUM_PLANES; i++) {
        SwCull::Plane p = frustum[i];
        float4 chosen;
        chosen.x = (p.mNormal.x >= 0) ? aabbMax.x : aabbMin.x;
        chosen.y = (p.mNormal.y >= 0) ? aabbMax.y : aabbMin.y;
        chosen.z = (p.mNormal.z >= 0) ? aabbMax.z : aabbMin.z;
        chosen.w = 1.0;
        float result = dot(p.mNormal, chosen.xyz) - p.mDistance;
        if (result < 0) {
            visible = false;
            break;
        }
    }
    return visible;
}

bool occlusionCull(inout float4[SwCull::NUM_AABB_CORNERS] aabbCorners, SwPerspective* perspective, Texture2D<float> depthPyramidImage, float2 drawExtents) {
    float4 aabbMinNdc = float4(1.f, 1.f, 1.f, 0.f);
    float4 aabbMaxNdc = float4(-1.f, -1.f, 0.f, 0.f);
    float closestDepthInRi = 0.f;

    // Transform AABB corners to NDC / Screen space and find min/max XY and closest Z
    // Vulkan NDC is [0, 1] for Z and [-1, 1] for XY
    // Reverse-Z in effect (1 == near, 0 == far)
    for (uint j = 0; j < SwCull::NUM_AABB_CORNERS; j++) {
        float4 clipPos = mul(perspective->mProj, mul(perspective->mView, aabbCorners[j]));

        // Corner is behind the near plane: w <= 0 means perspective division would flip
        // coordinates. Treat the whole object as visible (conservative fallback).
        if (clipPos.w <= 0.f)
            return true;

        float4 ndc = clipPos / clipPos.w;
        aabbCorners[j] = ndc;
        aabbMinNdc = min(aabbMinNdc, ndc);
        aabbMaxNdc = max(aabbMaxNdc, ndc);

        closestDepthInRi = max(closestDepthInRi, ndc.z);
    }

    // Convert NDC XY from [-1,1] to [0, drawExtents]
    float2 aabbMinScreen = ((aabbMinNdc.xy * float2(0.5f)) + 0.5f) * drawExtents;
    float2 aabbMaxScreen = ((aabbMaxNdc.xy * float2(0.5f)) + 0.5f) * drawExtents;

    // Convert from draw space to pyramid level-0 space before indexing.
    // The pyramid has pow2-floor dimensions which differ from the draw extent.
    float2 pyramidRatio = float2(workPc.mDepthPyramidExtents) / drawExtents;
    float2 aabbMinPyramid = aabbMinScreen * pyramidRatio;
    float2 aabbMaxPyramid = aabbMaxScreen * pyramidRatio;

    // Pick the mip where the footprint spans at most one level-0 texel (1.f guard avoids a
    // negative level when the footprint is sub-texel). At this level a single bilinear fetch
    // covers a 2x2 neighbourhood, so it fully overlaps the footprint.
    float2 footprint = aabbMaxPyramid - aabbMinPyramid;
    float level = ceil(log2(max(max(footprint.x, footprint.y), 1.f)));

    // Sample once at the footprint center. The min-reduction sampler returns the minimum (i.e.
    // furthest, in reverse-Z) depth across the covered texels — the furthest occluder.
    // Clamp the UV to [0,1] since the center can fall off-screen (negative) for items at the edge.
    float2 centerPyramid = (aabbMinPyramid + aabbMaxPyramid) * 0.5f;
    float2 uv = clamp(centerPyramid / float2(workPc.mDepthPyramidExtents), float2(0.f), float2(1.f));

    // Visible if the nearest point is closer-or-equal than the furthest occluder.
    // The depth pre-pass renders the instances being tested here, so frontmost surface finds its own depth in the pyramid
    float furthestDepthInPyramid = depthPyramidImage.SampleLevel(minSampler, uv, level);
    return closestDepthInRi >= furthestDepthInPyramid;
}

void markDrawRiIndex(SwRenderCommand* rc, SwRenderItem* ri, uint* sceneDrawRisIndicesBuffer) {
    // Write the indices of to-draw render items to the draw instances indices buffer
    // Index into to-draw instances indices buffer with (in.instanceIndex + firstInstance) and get the index into the instances buffer
    // Then index into the instances buffer with that index to get the actual instance data
    uint offsetVisibleRiIndex;
    InterlockedAdd(rc->mRiCount, 1, offsetVisibleRiIndex);
    uint visibleRiIndex = rc->mFirstRi + offsetVisibleRiIndex;
    sceneDrawRisIndicesBuffer[visibleRiIndex] = ri->mSceneInstanceIndex;
}

[numthreads(SwCull::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    uint risIndex = in.mDispatchThreadID.x;
    if (risIndex >= workPc.mRisLimit)
        return;
    SwRenderItem* ri = &workPc.mRisBuffer[risIndex];
    SwRenderCommand* rc = &workPc.mRcsBuffer[ri->mRcIndex];

    SwBounds* bounds = &workPc.mSceneBoundsBuffer[rc->mBoundsIndex];
    SwNodeTransform* nodeTransform = &workPc.mSceneNodeTransformsBuffer[rc->mNodeTransformIndex];
    SwInstance* instance = &workPc.mSceneInstancesBuffer[ri->mSceneInstanceIndex];
    SwCull::Plane* frustum = workPc.mFrustumBuffer;
    SwPerspective* perspective = &workPc.mPerFrameBuffer.mPerspective;
    float4[SwCull::NUM_AABB_CORNERS] aabbCorners = {
        float4(bounds->mMin.x, bounds->mMin.y, bounds->mMin.z, 1.f),
        float4(bounds->mMin.x, bounds->mMin.y, bounds->mMax.z, 1.f),
        float4(bounds->mMin.x, bounds->mMax.y, bounds->mMin.z, 1.f),
        float4(bounds->mMin.x, bounds->mMax.y, bounds->mMax.z, 1.f),
        float4(bounds->mMax.x, bounds->mMin.y, bounds->mMin.z, 1.f),
        float4(bounds->mMax.x, bounds->mMin.y, bounds->mMax.z, 1.f),
        float4(bounds->mMax.x, bounds->mMax.y, bounds->mMin.z, 1.f),
        float4(bounds->mMax.x, bounds->mMax.y, bounds->mMax.z, 1.f),
    };

    transformCornersToWorld(aabbCorners, instance, nodeTransform);

    uint riVisIndex = rc->mFirstRi + (ri->mSceneInstanceIndex - rc->mFirstInstance);

    bool draw = false;
    if (workPc.mPhase == SwCull::Phase::Early) {
        // Draw whatever was visible last frame (frustum only). Do not modify visibility.
        bool prevVisible = workPc.mSceneVisibilityRisReadBuffer[riVisIndex];
        if (prevVisible)
            prevVisible = !SwCull::ENABLE_FRUSTUM_CULL || frustumTest(frustum, aabbCorners);
        if (prevVisible) {
            markDrawRiIndex(rc, ri, workPc.mSceneDrawRisIndicesBuffer);
            draw = true;
        }
    } else {
        // Frustum + same-frame occlusion on all items. Write the result (0 OR 1) so occluded objects leave the visible set next frame, otherwise
        // anything once visible would be redrawn forever.
        // A disabled cull is treated as "passes" (visible). The static consts fold at compile time,
        // and short-circuiting keeps occlusion from running when the frustum test already failed.
        bool currVisible = (!SwCull::ENABLE_FRUSTUM_CULL || frustumTest(frustum, aabbCorners)) &&
                           (!SwCull::ENABLE_OCCLUSION_CULL || occlusionCull(aabbCorners, perspective, depthPyramidImage, workPc.mDrawExtents));
        workPc.mSceneVisibilityRisWriteBuffer[riVisIndex] = currVisible;

        // Draw only the newly-visible delta — anything that was visible last frame (read == 1)
        // was already drawn by the early pass. This only holds for batches that actually have an
        // early geometry pass (opaque), others (masked / transparent) must draw all visible here.
        bool drawnEarly = workPc.mHasEarlyDraw && workPc.mSceneVisibilityRisReadBuffer[riVisIndex];
        if (currVisible && !drawnEarly) {
            markDrawRiIndex(rc, ri, workPc.mSceneDrawRisIndicesBuffer);
            draw = true;
        }
    }
    uint _;
    if (draw)
        InterlockedAdd(*workPc.mRisCount, 1, _);
}
