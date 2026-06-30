import SwLighting;
import SwCommon;
import SwCull;

[[vk::push_constant]]
SwLighting::ShadowCullPC shadowCullPc;

static const uint NUM_CORNERS = 8;

void buildWorldCorners(Sw::Bounds bounds, float4x4 model, out float4 corners[NUM_CORNERS]) {
    corners[0] = float4(bounds.mMin.x, bounds.mMin.y, bounds.mMin.z, 1.f);
    corners[1] = float4(bounds.mMin.x, bounds.mMin.y, bounds.mMax.z, 1.f);
    corners[2] = float4(bounds.mMin.x, bounds.mMax.y, bounds.mMin.z, 1.f);
    corners[3] = float4(bounds.mMin.x, bounds.mMax.y, bounds.mMax.z, 1.f);
    corners[4] = float4(bounds.mMax.x, bounds.mMin.y, bounds.mMin.z, 1.f);
    corners[5] = float4(bounds.mMax.x, bounds.mMin.y, bounds.mMax.z, 1.f);
    corners[6] = float4(bounds.mMax.x, bounds.mMax.y, bounds.mMin.z, 1.f);
    corners[7] = float4(bounds.mMax.x, bounds.mMax.y, bounds.mMax.z, 1.f);
    for (uint i = 0; i < NUM_CORNERS; i++) {
        corners[i] = mul(model, corners[i]);
    }
}

// At least one corner on the inward side of all six planes means the world-space AABB intersects the frustum.
bool frustumTest(SwCull::Plane* frustum, float4 corners[NUM_CORNERS]) {
    float4 aabbMin = corners[0];
    float4 aabbMax = corners[0];
    for (uint j = 1; j < NUM_CORNERS; j++) {
        aabbMin = min(aabbMin, corners[j]);
        aabbMax = max(aabbMax, corners[j]);
    }

    for (uint i = 0; i < SwCull::NUM_FRUSTUM_PLANES; i++) {
        SwCull::Plane p = frustum[i];
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
bool rangeTest(float4 corners[NUM_CORNERS], float3 center, float range) {
    if (range <= 0.f) {
        return true;  // infinite range, nothing to cull
    }
    float3 aabbMin = corners[0].xyz;
    float3 aabbMax = corners[0].xyz;
    for (uint j = 1; j < NUM_CORNERS; j++) {
        aabbMin = min(aabbMin, corners[j].xyz);
        aabbMax = max(aabbMax, corners[j].xyz);
    }
    float3 closest = clamp(center, aabbMin, aabbMax);
    float3 delta = closest - center;
    return dot(delta, delta) <= range * range;
}

[numthreads(Sw::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint risIndex = in.mDispatchThreadID.x;
    if (risIndex >= shadowCullPc.mRisCount) {
        return;
    }

    Sw::RenderItem ri = shadowCullPc.mRisBuffer[risIndex];
    Sw::RenderCommand* rc = &shadowCullPc.mLightRcsBuffer[ri.mRcIndex];

    Sw::Bounds bounds = shadowCullPc.mSceneBoundsBuffer[rc->mBoundsIndex];
    float4x4 nodeTransform = shadowCullPc.mSceneNodeTransformsBuffer[rc->mNodeTransformIndex];
    float4x4 instanceTransform = shadowCullPc.mSceneInstancesBuffer[ri.mSceneInstanceIndex].mTransformMatrix;
    float4x4 model = mul(instanceTransform, nodeTransform);

    float4 corners[NUM_CORNERS];
    buildWorldCorners(bounds, model, corners);

    bool visible = true;
    if (shadowCullPc.mLightType != Sw::LightType::Point) {
        visible = frustumTest(shadowCullPc.mFrustumBuffer, corners);
    }
    if (visible && shadowCullPc.mLightType != Sw::LightType::Directional) {
        visible = rangeTest(corners, shadowCullPc.mLightWorldPos, shadowCullPc.mLightRange);
    }

    if (visible) {
        uint offset;
        InterlockedAdd(rc->mRiCount, 1, offset);
        shadowCullPc.mLightDrawRisIndicesBuffer[rc->mFirstRi + offset] = ri.mSceneInstanceIndex;
    }
}
