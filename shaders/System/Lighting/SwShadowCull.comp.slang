import System.Lighting.SwLighting;
import Data.SwConstants;
import System.Cull.SwCull;

[[vk::push_constant]]
SwLighting::ShadowCullPC shadowCullPc;

void constructCorners(SwMesh::Bounds* riBounds, float4x4 riModelMatrix, out float4 corners[SwMesh::NUM_AABB_CORNERS]) {
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

// Spot light perspective frustum whose four side planes pass through the apex ("tip" of the frustum).
// Directional lights have no intrinsic bounds to cull against, so they skip this test entirely.
void constructFrustum(SwLight::Data* light, float3 apex, float3 forward, out SwFrustum::Plane frustum[SwFrustum::NUM_FRUSTUM_PLANES]) {
    float3 up = (abs(forward.y) < 0.999f) ? float3(0.f, 1.f, 0.f) : float3(1.f, 0.f, 0.f);
    float3 right = normalize(cross(forward, up));
    up = cross(right, forward);

    float outerAngle = acos(clamp(light->mOuterCos, -0.9999f, 0.9999f));
    float aspect = 1.f;
    float halfVSide = tan(outerAngle);
    float halfHSide = halfVSide * aspect;
    float near = 0.05f;
    float far = (light->mRange > 0.f) ? light->mRange : 60.f;

    frustum[0].mNormal = forward;
    frustum[0].mDistance = dot(forward, apex + forward * near);
    frustum[1].mNormal = -forward;
    frustum[1].mDistance = dot(-forward, apex + forward * far);
    frustum[2].mNormal = normalize(cross(forward - right * halfHSide, up));
    frustum[2].mDistance = dot(frustum[2].mNormal, apex);
    frustum[3].mNormal = normalize(cross(up, forward + right * halfHSide));
    frustum[3].mDistance = dot(frustum[3].mNormal, apex);
    frustum[4].mNormal = normalize(cross(forward + up * halfVSide, right));
    frustum[4].mDistance = dot(frustum[4].mNormal, apex);
    frustum[5].mNormal = normalize(cross(right, forward - up * halfVSide));
    frustum[5].mDistance = dot(frustum[5].mNormal, apex);
}

// At least one corner on the inward side of all six planes means the world-space AABB intersects the frustum.
bool frustumTest(SwFrustum::Plane frustum[SwFrustum::NUM_FRUSTUM_PLANES], float4 corners[SwMesh::NUM_AABB_CORNERS]) {
    float4 aabbMin = corners[0];
    float4 aabbMax = corners[0];
    for (uint j = 1; j < SwMesh::NUM_AABB_CORNERS; j++) {
        aabbMin = min(aabbMin, corners[j]);
        aabbMax = max(aabbMax, corners[j]);
    }

    for (uint i = 0; i < SwFrustum::NUM_FRUSTUM_PLANES; i++) {
        SwFrustum::Plane p = frustum[i];
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
    if (risIndex >= shadowCullPc.mShadowRisLimit) {
        return;
    }

    SwBatch::RenderItem* ri = &shadowCullPc.mShadowRisBuffer[risIndex];
    SwBatch::RenderCommand* rc = &shadowCullPc.mShadowRcsBuffer[ri->mRcIndex];

    SwMesh::Bounds* riBounds = &shadowCullPc.mSceneBoundsBuffer[rc->mBoundsIndex];
    float4x4 riNodeTransform = shadowCullPc.mSceneNodeTransformsBuffer[rc->mNodeTransformIndex];
    float4x4 riInstanceTransform = shadowCullPc.mSceneInstancesBuffer[ri->mSceneInstanceIndex].mTransformMatrix;
    float4x4 riModelMatrix = mul(riInstanceTransform, riNodeTransform);

    uint sceneLightsIndex = shadowCullPc.mActiveLightsBuffer->mActiveLightIndices[shadowCullPc.mLightIndex];
    SwLight::Data* light = &shadowCullPc.mSceneLightsBuffer[sceneLightsIndex];
    float4x4 lightNodeTransform = shadowCullPc.mSceneNodeTransformsBuffer[light->mNodeTransformIndex]; 
    float4x4 lightInstanceTransform = shadowCullPc.mSceneInstancesBuffer[light->mInstanceIndex].mTransformMatrix; 
    float4x4 lightModelMatrix = mul(lightInstanceTransform, lightNodeTransform);
    float3 lightApex = mul(lightModelMatrix, float4(light->mBasePosition, 1.f)).xyz;
    float3 lightForward = normalize(mul(lightModelMatrix, float4(light->mBaseDirection, 0.f)).xyz);

    float4 corners[SwMesh::NUM_AABB_CORNERS];
    constructCorners(riBounds, riModelMatrix, corners);

    bool visible = true;
    if (light->mType == SwLight::Type::Spot) {
        SwFrustum::Plane frustum[SwFrustum::NUM_FRUSTUM_PLANES];
        constructFrustum(light, lightApex, lightForward, frustum);
        visible = frustumTest(frustum, corners);
    }
    if (visible && light->mType != SwLight::Type::Directional) {
        visible = rangeTest(corners, lightApex, light->mRange);
    }

    if (visible) {
        uint offset;
        InterlockedAdd(rc->mRiCount, 1, offset);
        shadowCullPc.mShadowRisIndicesBuffer[rc->mFirstRi + offset] = ri->mSceneInstanceIndex;
    }
}
