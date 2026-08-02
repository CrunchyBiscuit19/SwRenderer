import System.Lighting.SwLighting;
import Data.SwConstants;
import System.Cull.SwCull;

[[vk::push_constant]]
SwLighting::ShadowsCullPC shadowsCullPc;

void constructCorners(SwBounds riBounds, float4x4 riModelMatrix, out float4 corners[SwMesh::NUM_AABB_CORNERS]) {
    corners[0] = float4(riBounds.mMin.x, riBounds.mMin.y, riBounds.mMin.z, 1.f);
    corners[1] = float4(riBounds.mMin.x, riBounds.mMin.y, riBounds.mMax.z, 1.f);
    corners[2] = float4(riBounds.mMin.x, riBounds.mMax.y, riBounds.mMin.z, 1.f);
    corners[3] = float4(riBounds.mMin.x, riBounds.mMax.y, riBounds.mMax.z, 1.f);
    corners[4] = float4(riBounds.mMax.x, riBounds.mMin.y, riBounds.mMin.z, 1.f);
    corners[5] = float4(riBounds.mMax.x, riBounds.mMin.y, riBounds.mMax.z, 1.f);
    corners[6] = float4(riBounds.mMax.x, riBounds.mMax.y, riBounds.mMin.z, 1.f);
    corners[7] = float4(riBounds.mMax.x, riBounds.mMax.y, riBounds.mMax.z, 1.f);
    for (uint i = 0; i < SwMesh::NUM_AABB_CORNERS; i++) {
        corners[i] = mul(riModelMatrix, corners[i]);
    }
}

bool rangeTest(float3 aabbMin, float3 aabbMax, float3 center, float range) {
    if (range <= 0.f) {
        return true;
    }
    float3 closest = clamp(center, aabbMin, aabbMax);
    float3 delta = closest - center;
    return dot(delta, delta) <= range * range;
}

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint viewIndex = in.mDispatchThreadID.x;
    if (viewIndex >= SwLighting::Shadow::MAX_NUM_SHADOW_VIEWS) {
        return;
    }
    SwLighting::Shadow::View view = shadowsCullPc.mShadowsViewsBuffer[viewIndex];
    
    uint lightIndex = view.mLightIndex;
    if (lightIndex == 0xFFFFFFFFu) {
        return;
    }

    uint risIndex = in.mDispatchThreadID.y;
    if (risIndex >= shadowsCullPc.mShadowsRisLimit) {
        return;
    }

    SwRenderItem* ri = &shadowsCullPc.mRisBuffer[risIndex];
    SwRenderCommand* rc = &shadowsCullPc.mShadowsRcsBuffer[viewIndex * shadowsCullPc.mNumRcsPerShadowView + ri->mRcIndex];

    SwBounds riBounds = shadowsCullPc.mBoundsBuffer[rc->mBoundsIndex];
    float4x4 riNodeTransform = shadowsCullPc.mNodeTransformsBuffer[rc->mNodeTransformIndex];
    float4x4 riInstanceTransform = shadowsCullPc.mInstancesBuffer[ri->mInstanceIndex].mTransformMatrix;
    float4x4 riModelMatrix = mul(riInstanceTransform, riNodeTransform);

    SwLight::Data light = shadowsCullPc.mLightsBuffer[lightIndex]; 
    float4x4 lightNodeTransform = shadowsCullPc.mNodeTransformsBuffer[light.mNodeTransformIndex];
    float4x4 lightInstanceTransform = shadowsCullPc.mInstancesBuffer[light.mInstanceIndex].mTransformMatrix;
    float4x4 lightModelMatrix = mul(lightInstanceTransform, lightNodeTransform);
    float3 lightApex = mul(lightModelMatrix, float4(light.mBasePosition, 1.f)).xyz;

    float4 corners[SwMesh::NUM_AABB_CORNERS];
    constructCorners(riBounds, riModelMatrix, corners);

    float3 aabbMin;
    float3 aabbMax;
    SwBounds::fromCorners(corners, aabbMin, aabbMax);

    bool visible = true;
    if (light.mType == SwLight::Type::Spot || light.mType == SwLight::Type::Point) {
        
        //SwFrustum frustumData =  TODO: source the rebuilt per-light frustum
        //visible = SwFrustum::aabbInFrustum(frustumData, aabbMin, aabbMax);
    }
    if (visible && light.mType != SwLight::Type::Directional) {
        visible = rangeTest(aabbMin, aabbMax, lightApex, light.mRange);
    }

    if (visible) {
        uint offset;
        InterlockedAdd(rc->mRiCount, 1, offset);
        shadowsCullPc.mShadowsRisIndicesBuffer[viewIndex * shadowsCullPc.mNumRisPerShadowView + (rc->mFirstRi + offset)] = ri->mInstanceIndex;
    }
}
