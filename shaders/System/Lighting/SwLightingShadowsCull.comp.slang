import System.Lighting.SwLighting;
import Data.SwConstants;
import System.Cull.SwCull;

[[vk::push_constant]]
SwLighting::ShadowsCullPC shadowsCullPc;

void constructCorners(SwBounds* riBounds, float4x4 riModelMatrix, out float4 corners[SwMesh::NUM_AABB_CORNERS]) {
    corners[0] = float4(riBounds->mMin.x, riBounds->mMin.y, riBounds->mMin.z, 1.f);
    corners[1] = float4(riBounds->mMin.x, riBounds->mMin.y, riBounds->mMax.z, 1.f);
    corners[2] = float4(riBounds->mMin.x, riBounds->mMax.y, riBounds->mMin.z, 1.f);
    corners[3] = float4(riBounds->mMin.x, riBounds->mMax.y, riBounds->mMax.z, 1.f);
    corners[4] = float4(riBounds->mMax.x, riBounds->mMin.y, riBounds->mMin.z, 1.f);
    corners[5] = float4(riBounds->mMax.x, riBounds->mMin.y, riBounds->mMax.z, 1.f);
    corners[6] = float4(riBounds->mMax.x, riBounds->mMax.y, riBounds->mMin.z, 1.f);
    corners[7] = float4(riBounds->mMax.x, riBounds->mMax.y, riBounds->mMax.z, 1.f);
    for (uint i = 0; i < SwMesh::NUM_AABB_CORNERS; i++) {
        corners[i] = mul(riModelMatrix, corners[i]);
    }
}

// At least one corner on the inward side of all six planes means the world-space AABB intersects the frustum.
bool frustumTest(SwFrustum frustum, float4 corners[SwMesh::NUM_AABB_CORNERS]) {
    float4 aabbMin = corners[0];
    float4 aabbMax = corners[0];
    for (uint j = 1; j < SwMesh::NUM_AABB_CORNERS; j++) {
        aabbMin = min(aabbMin, corners[j]);
        aabbMax = max(aabbMax, corners[j]);
    }

    SwPlane* planes[SwFrustum::NUM_FRUSTUM_PLANES] = {
        &frustum.mNear, &frustum.mFar, &frustum.mLeft, &frustum.mRight, &frustum.mTop, &frustum.mBottom
    };

    for (uint i = 0; i < SwFrustum::NUM_FRUSTUM_PLANES; i++) {
        SwPlane p = *planes[i];
        float3 chosen;
        chosen.x = (p.mNormal.x >= 0) ? aabbMax.x : aabbMin.x;
        chosen.y = (p.mNormal.y >= 0) ? aabbMax.y : aabbMin.y;
        chosen.z = (p.mNormal.z >= 0) ? aabbMax.z : aabbMin.z;
        if (dot(p.mNormal, chosen) - p.mDistance < 0) {
            return false;
        }
    }
    return true;
}

// The closest point on the world-space AABB to the light is within range means the light can reach the caster.
bool rangeTest(float4 corners[SwMesh::NUM_AABB_CORNERS], float3 center, float range) {
    if (range <= 0.f) {
        return true;  
    }
    float3 aabbMin = corners[0].xyz;
    float3 aabbMax = corners[0].xyz;
    for (uint j = 1; j < SwMesh::NUM_AABB_CORNERS; j++) {
        aabbMin = min(aabbMin, corners[j].xyz);
        aabbMax = max(aabbMax, corners[j].xyz);
    }
    float3 closest = clamp(center, aabbMin, aabbMax);
    float3 delta = closest - center;
    return dot(delta, delta) <= range * range;
}

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint risIndex = in.mDispatchThreadID.x;
    if (risIndex >= shadowsCullPc.mShadowsRisLimit) {
        return;
    }

    SwRenderItem* ri = &shadowsCullPc.mShadowsRisBuffer[risIndex];
    SwRenderCommand* rc = &shadowsCullPc.mShadowsRcsBuffer[ri->mRcIndex];

    SwBounds* riBounds = &shadowsCullPc.mSceneBoundsBuffer[rc->mBoundsIndex];
    float4x4 riNodeTransform = shadowsCullPc.mSceneNodeTransformsBuffer[rc->mNodeTransformIndex];
    float4x4 riInstanceTransform = shadowsCullPc.mSceneInstancesBuffer[ri->mInstanceIndex].mTransformMatrix;
    float4x4 riModelMatrix = mul(riInstanceTransform, riNodeTransform);

    uint activeLightIndex = 0;  // TODO: source the per-dispatch shadow-caster index once the cull dispatch is rebuilt
    uint sceneLightsIndex = activeLightIndex;  // TODO: map the shadow-caster slot to its scene light index
    SwLight::Data* light = &shadowsCullPc.mSceneLightsBuffer[sceneLightsIndex];
    float4x4 lightNodeTransform = shadowsCullPc.mSceneNodeTransformsBuffer[light->mNodeTransformIndex]; 
    float4x4 lightInstanceTransform = shadowsCullPc.mSceneInstancesBuffer[light->mInstanceIndex].mTransformMatrix; 
    float4x4 lightModelMatrix = mul(lightInstanceTransform, lightNodeTransform);
    float3 lightApex = mul(lightModelMatrix, float4(light->mBasePosition, 1.f)).xyz;

    float4 corners[SwMesh::NUM_AABB_CORNERS];
    constructCorners(riBounds, riModelMatrix, corners);

    bool visible = true;
    if (light->mType == SwLight::Type::Spot) {
        //SwFrustum frustumData = ...;  // TODO: source the rebuilt per-light frustum
        //visible = frustumTest(frustumData, corners);
    }
    if (visible && light->mType != SwLight::Type::Directional) {
        visible = rangeTest(corners, lightApex, light->mRange);
    }

    if (visible) {
        uint offset;
        InterlockedAdd(rc->mRiCount, 1, offset);
        shadowsCullPc.mShadowsRisIndicesBuffer[rc->mFirstRi + offset] = ri->mInstanceIndex;
    }
}
