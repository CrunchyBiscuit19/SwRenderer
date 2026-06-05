import SwCull;
import SwCommon;

[[vk::push_constant]]
SwCull::WorkPC workPc;

bool frustumCull(
    SwCull::Plane* frustum, inout float4[SwCull::NUM_AABB_CORNERS] aabbCorners, SwBounds* bounds, SwInstance* currInstance, SwNodeTransform* nodeTransform
) {
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

[numthreads(SwCull::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    // Get thread ID as index into which render instance for culling check
    uint renderInstancesIndex = in.mDispatchThreadID.x;
    if (renderInstancesIndex >= workPc.mRenderInstancesLimit)
        return;

    SwRenderInstance renderInstance = workPc.mRenderInstancesBuffer[renderInstancesIndex];
    SwRenderItem* renderItem = &workPc.mRenderItemsBuffer[renderInstance.mRenderItemsIndex];

    SwBounds* bounds = &workPc.mSceneBoundsBuffer[renderItem->mBoundsIndex];
    SwNodeTransform* nodeTransform = &workPc.mSceneNodeTransformsBuffer[renderItem->mNodeTransformIndex];
    SwInstance* instance = &workPc.mSceneInstancesBuffer[renderInstance.mSceneInstanceIndex];
    SwCull::Plane* frustum = workPc.mFrustumBuffer;
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

    bool visible = frustumCull(frustum, aabbCorners, bounds, instance, nodeTransform);

    if (!visible)
        return;
    uint _;
    InterlockedAdd(*workPc.mRenderInstancesCountBuffer, 1, _);  // CAUTION * or [0]

    // Write the indices of visible render instances to the visible instances indices buffer
    // Index into visible instances indices buffer with (in.instanceIndex + firstInstance) and get the index into the instances buffer
    // Then index into the instances buffer with that index to get the actual instance data
    uint offsetVisibleRenderInstanceIndex;
    InterlockedAdd(renderItem->mRenderInstanceCount, 1, offsetVisibleRenderInstanceIndex);  // Reset renderItem->mRenderInstanceCount to 0 before occlusion?
    uint finalVisibleRenderInstanceIndex = renderItem->mFirstRenderInstance + offsetVisibleRenderInstanceIndex;
    workPc.mSceneVisibleRenderInstancesIndicesBuffer[finalVisibleRenderInstanceIndex] = renderInstance.mSceneInstanceIndex;  // Reset this as well?
}
