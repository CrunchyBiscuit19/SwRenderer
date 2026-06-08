import SwCull;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthPyramidImage;

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

    // floor+clamp to [0, pyramidExtent] before converting to uint.
    // Negative values arise when the AABB projects off the top-left of the screen;
    // converting a negative float to uint is undefined behaviour.
    float2 pyramidExtentF = float2(workPc.mDepthPyramidExtents);
    uint2 pMin = uint2(clamp(floor(aabbMinPyramid), float2(0.f), pyramidExtentF));
    uint2 pMax = uint2(clamp(floor(aabbMaxPyramid), float2(0.f), pyramidExtentF));

    // Compute information needed to look up in the depth pyramid
    uint width = max(pMax.x - pMin.x, 1u);
    uint height = max(pMax.y - pMin.y, 1u);
    uint2 topLeft = pMin;
    uint2 topRight = uint2(pMax.x, pMin.y);
    uint2 bottomLeft = uint2(pMin.x, pMax.y);
    uint2 bottomRight = pMax;

    // Adjusted for levels, then clamp within the mip size
    uint level = uint(floor(log2(float(max(width, height)))));
    uint2 mipSize = (workPc.mDepthPyramidExtents + (1u << level) - 1u) >> level;
    uint2 clampedTopLeft = clamp(topLeft >> level, uint2(0, 0), mipSize - 1u);
    uint2 clampedTopRight = clamp(topRight >> level, uint2(0, 0), mipSize - 1u);
    uint2 clampedBottomLeft = clamp(bottomLeft >> level, uint2(0, 0), mipSize - 1u);
    uint2 clampedBottomRight = clamp(bottomRight >> level, uint2(0, 0), mipSize - 1u);

    // Sample the four corners of the AABB footprint at the selected mip level
    float d0 = depthPyramidImage.Load(uint3(clampedTopRight.x, clampedTopRight.y, level));
    float d1 = depthPyramidImage.Load(uint3(clampedTopLeft.x, clampedTopLeft.y, level));
    float d2 = depthPyramidImage.Load(uint3(clampedBottomLeft.x, clampedBottomLeft.y, level));
    float d3 = depthPyramidImage.Load(uint3(clampedBottomRight.x, clampedBottomRight.y, level));

    // Visible if the nearest point is closer-or-equal than the furthest occluder.
    // The depth pre-pass renders the instances being tested here, so frontmost surface finds its own depth in the pyramid
    float furthestDepthInPyramid = min(min(d0, d1), min(d2, d3));
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
        // Draw whatever was visible last frame (frustum only). The early pass trusts last
        // frame's occlusion result and does NOT touch the visibility buffer — the late pass
        // owns the authoritative visibility for the next frame.
        bool prevVisible = (workPc.mSceneVisibilityRisReadBuffer[riVisIndex] == 1);
        if (prevVisible)
            prevVisible = frustumTest(frustum, aabbCorners);
        if (prevVisible) {
            markDrawRiIndex(rc, ri, workPc.mSceneDrawRisIndicesBuffer);
            draw = true;
        }
    } else {
        // Frustum + same-frame occlusion on all items. This is the authoritative test: write
        // the result (0 OR 1) so occluded objects leave the visible set next frame, otherwise
        // anything once visible would be redrawn forever and occlusion would cull nothing.
        bool currVisible = frustumTest(frustum, aabbCorners) && occlusionCull(aabbCorners, perspective, depthPyramidImage, workPc.mDrawExtents);
        workPc.mSceneVisibilityRisWriteBuffer[riVisIndex] = currVisible ? 1u : 0u;

        // Draw only the newly-visible delta — anything that was visible last frame (read == 1)
        // was already drawn by the early pass. This only holds for batches that actually have an
        // early geometry pass (opaque); masked/transparent are drawn solely here, so they must
        // emit the full visible set every frame.
        bool drawnEarly = (workPc.mHasEarlyDraw != 0) && (workPc.mSceneVisibilityRisReadBuffer[riVisIndex] == 1);
        if (currVisible && !drawnEarly) {
            markDrawRiIndex(rc, ri, workPc.mSceneDrawRisIndicesBuffer);
            draw = true;
        }
    }
    uint _;
    if (draw)
        InterlockedAdd(*workPc.mRisCount, 1, _);
}
