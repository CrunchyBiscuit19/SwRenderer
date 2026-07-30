import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::LightsCullPC lightsCullPc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint lightIndex = in.mDispatchThreadID.x;
    if (lightIndex >= lightsCullPc.mLightsLimit) {
        return;
    }

    SwLight::Data light = lightsCullPc.mLightsBuffer[lightIndex];

    float3 center;
    float radius;
    bool visible = true;
    if (SwLighting::Collision::lightBoundingSphere(light, lightsCullPc.mInstancesBuffer, lightsCullPc.mNodeTransformsBuffer, center, radius)) {
        SwFrustum frustum = lightsCullPc.mFrameBuffer->mCameraBuffer->mFrustum;
        visible = SwFrustum::sphereInFrustum(frustum, center, radius);
    }

    if (visible) {
        uint offset;
        InterlockedAdd(lightsCullPc.mLightsVisibleIndicesBuffer[0], 1, offset);  
        lightsCullPc.mLightsVisibleIndicesBuffer[1 + offset] = lightIndex;

        uint shadowIndex;
        InterlockedAdd(lightsCullPc.mShadowMapSlotsCount[(uint)light.mType], 1, shadowIndex);
        lightsCullPc.mLightsBuffer[lightIndex].mShadowMapSlot = uint2((uint)light.mType, shadowIndex);
        // If shadowIndex overflows the descriptor array of shadow maps, ignore it during geometry draws.

        switch (light.mType) {
            case SwLight::Type::Directional:
                if (shadowIndex >= SwLighting::Shadow::MAX_DIRECTIONAL_SHADOW_MAPS) break;
                lightsCullPc.mShadowsLightIndicesBuffer[SwLighting::Shadow::DIRECTIONAL_SHADOW_MAP_BASE + shadowIndex] = lightIndex;
                break;
            case SwLight::Type::Point:
                if (shadowIndex >= SwLighting::Shadow::MAX_POINT_SHADOW_MAPS) break;
                lightsCullPc.mShadowsLightIndicesBuffer[SwLighting::Shadow::POINT_SHADOW_MAP_BASE + shadowIndex] = lightIndex;
                break;
            case SwLight::Type::Spot:
                if (shadowIndex >= SwLighting::Shadow::MAX_SPOT_SHADOW_MAPS) break;
                lightsCullPc.mShadowsLightIndicesBuffer[SwLighting::Shadow::SPOT_SHADOW_MAP_BASE + shadowIndex] = lightIndex;
                break;
        }
    }
}
