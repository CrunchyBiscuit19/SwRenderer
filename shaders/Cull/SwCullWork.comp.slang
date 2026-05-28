import SwCull;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthPyramidImage;

[[vk::push_constant]]
SwCull::WorkPC workPc;

bool frustumCull(SwCull::Plane *frustum, inout float4[SwCull::NUM_AABB_CORNERS] aabbCorners, SwBounds* bounds, SwInstance* currInstance, SwNodeTransform* nodeTransform) {
    bool visible = true;

    // Transform all corners of the AABB to world space
    // Get the min and max corners of the AABB
    aabbCorners[0] = mul(currInstance->mTransformMatrix, mul(*nodeTransform, aabbCorners[0]));
    float4 aabbMin = aabbCorners[0];
    float4 aabbMax = aabbCorners[0];
    for (uint j = 1; j < SwCull::NUM_AABB_CORNERS; j++) {
        aabbCorners[j] = mul(currInstance->mTransformMatrix, mul(*nodeTransform, aabbCorners[j]));
        aabbMin = min(aabbMin, aabbCorners[j]);
        aabbMax = max(aabbMax, aabbCorners[j]);
    }

    // For each plane build the corner which will produce max result in point-normal plane equation
    // Point-normal plane equation is positive if it's on the side of the plane the normal is pointing
    // Frustum plane normals point inward => non-negative result means at least 1 corner is on correct side of the plane
    // At least 1 corner on correct side of all 6 planes means render instance is within frustum and therefore visible
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

bool occlusionCull(inout float4[SwCull::NUM_AABB_CORNERS] aabbCorners, SwPerspective *perspective, Texture2D<float> depthPyramidImage, float2 drawExtents) {
    float4 aabbMinNdc = float4(1.f, 1.f, 1.f, 0.f);
    float4 aabbMaxNdc = float4(-1.f, -1.f, 0.f, 0.f);
    float closestDepthInRenderInstance = 0.f;

    // Transform AABB corners to NDC / Screen space and find min/max XY and closest Z
    // Vulkan NDC is [0, 1] for Z and [-1, 1] for XY
    // Reverse-Z in effect (1 == near, 0 == far)
    for (uint j = 0; j < SwCull::NUM_AABB_CORNERS; j++) {
        float4 clipPos = mul(perspective->mProj, mul(perspective->mView, aabbCorners[j]));

        float4 ndc = clipPos / clipPos.w;
        aabbCorners[j] = ndc;
        aabbMinNdc = min(aabbMinNdc, ndc);
        aabbMaxNdc = max(aabbMaxNdc, ndc);

        closestDepthInRenderInstance = max(closestDepthInRenderInstance, ndc.z);
    }

    // Convert NDC XY from [-1,1] to [0, drawExtents], and clamp it
    // Clamping because AABB spills outside of screen, also increase margin to be safe
    float2 aabbMinScreen = ((aabbMinNdc.xy * float2(0.5f)) + 0.5f) * drawExtents;
    float2 aabbMaxScreen = ((aabbMaxNdc.xy * float2(0.5f)) + 0.5f) * drawExtents;

    // Compute information needed to look up in the depth pyramid
    uint width = max(uint(aabbMaxScreen.x - aabbMinScreen.x), 1u);
    uint height = max(uint(aabbMaxScreen.y - aabbMinScreen.y), 1u);
    uint2 topLeft = uint2((uint)aabbMinScreen.x, (uint)aabbMinScreen.y);
    uint2 topRight = uint2((uint)aabbMaxScreen.x, (uint)aabbMinScreen.y);
    uint2 bottomLeft = uint2((uint)aabbMinScreen.x, (uint)aabbMaxScreen.y);
    uint2 bottomRight = uint2((uint)aabbMaxScreen.x, (uint)aabbMaxScreen.y);

    // Adjusted for levels, then clamp within the mip size
    uint level = uint(floor(log2(float(max(width, height)))));
    uint2 mipSize = (workPc.mDepthPyramidExtents + (1u << level) - 1u) >> level;
    uint2 clampedTopLeft = clamp(topLeft >> level, uint2(0, 0), mipSize - 1u);
    uint2 clampedTopRight = clamp(topRight >> level, uint2(0, 0), mipSize - 1u);
    uint2 clampedBottomLeft = clamp(bottomLeft >> level, uint2(0, 0), mipSize - 1u);
    uint2 clampedBottomRight = clamp(bottomRight >> level, uint2(0, 0), mipSize - 1u);

    // Sample the pyramid using the center coordinates adjusted for mip level
    // Not perfect but whatever
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
    if (renderInstancesIndex >= workPc.mRenderInstancesLimit) return;

    // Grab representative data for this thread
    SwRenderInstance renderInstance = workPc.mRenderInstancesBuffer[renderInstancesIndex];
    SwRenderItem *renderItem = &workPc.mPreCullRenderItemsBuffer[renderInstance.mRenderItemsIndex];

    // Grab data for culling test
    SwInstance* instance = &workPc.mSceneInstancesBuffer[renderInstance.mSceneInstanceIndex];
    SwNodeTransform* nodeTransform = &workPc.mSceneNodeTransformsBuffer[renderItem->mNodeTransformIndex];
    SwBounds* bounds = &workPc.mSceneBoundsBuffer[renderItem->mBoundsIndex];
    SwCull::Plane* frustum = workPc.mFrustumBuffer;
    SwPerspective* perspective = &workPc.mPerFrameBuffer.mPerspective;

    // Establish AABB corners
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

    // Culling tests
    bool visible = frustumCull(frustum, aabbCorners, bounds, instance, nodeTransform);
    // if (visible) visible = occlusionCull(aabbCorners, perspective, depthPyramidImage, workPc.drawExtents);
    if (visible) occlusionCull(aabbCorners, perspective, depthPyramidImage, workPc.mDrawExtents); // take the penalty hit without actually doing occlusion

    // Increment counter of unculled render instances
    if (!visible) return;
    uint _; InterlockedAdd(workPc.mRenderInstancesCountBuffer[0], 1, _);

    // Write the indices of visible render instances to the visible instances indices buffer
    // Index into visible instances indices buffer with (in.instanceIndex + firstInstance) and get the index into the instances buffer
    // Then index into the instances buffer with that index to get the actual instance data
    uint offsetVisibleRenderInstanceIndex;
    InterlockedAdd(renderItem->mRenderInstanceCount, 1, offsetVisibleRenderInstanceIndex);
    uint finalVisibleRenderInstanceIndex = renderItem->mFirstRenderInstance + offsetVisibleRenderInstanceIndex;
    workPc.mSceneVisibleRenderInstancesInstanceIndexBuffer[finalVisibleRenderInstanceIndex] = renderInstance.mSceneInstanceIndex;
}