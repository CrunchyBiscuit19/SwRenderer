import SwCull;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthPyramidImage;

[[vk::push_constant]]
SwCull::WorkPC workPc;

bool occlusionCull(inout float4[SwCull::NUM_AABB_CORNERS] aabbCorners, SwPerspective* perspective, Texture2D<float> depthPyramidImage, float2 drawExtents) {
    float4 aabbMinNdc = float4(1.f, 1.f, 1.f, 0.f);
    float4 aabbMaxNdc = float4(-1.f, -1.f, 0.f, 0.f);
    float closestDepthInRenderInstance = 0.f;

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

        closestDepthInRenderInstance = max(closestDepthInRenderInstance, ndc.z);
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

    // Visible if closest is closer (greater than because of reverse-Z) than furthest depth
    float furthestDepthInPyramid = min(min(d0, d1), min(d2, d3));
    return closestDepthInRenderInstance > furthestDepthInPyramid;
}

[numthreads(SwCull::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    // Get thread ID as index into which render instance for culling check
    uint renderInstancesIndex = in.mDispatchThreadID.x;
    if (renderInstancesIndex >= workPc.mRenderInstancesLimit)
        return;

    SwRenderInstance renderInstance = workPc.mRenderInstancesBuffer[renderInstancesIndex];
    SwRenderItem* renderItem = &workPc.mPreCullRenderItemsBuffer[renderInstance.mRenderItemsIndex];

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

    bool visible = occlusionCull(aabbCorners, perspective, depthPyramidImage, workPc.mDrawExtents);

    if (!visible)
        return;
    uint _;
    InterlockedAdd(*workPc.mRenderInstancesCountBuffer, 1, _);

    // Write the indices of visible render instances to the visible instances indices buffer
    // Index into visible instances indices buffer with (in.instanceIndex + firstInstance) and get the index into the instances buffer
    // Then index into the instances buffer with that index to get the actual instance data
    uint offsetVisibleRenderInstanceIndex;
    InterlockedAdd(renderItem->mRenderInstanceCount, 1, offsetVisibleRenderInstanceIndex);
    uint finalVisibleRenderInstanceIndex = renderItem->mFirstRenderInstance + offsetVisibleRenderInstanceIndex;
    workPc.mSceneVisibleRenderInstancesIndicesBuffer[finalVisibleRenderInstanceIndex] = renderInstance.mSceneInstanceIndex;
}
