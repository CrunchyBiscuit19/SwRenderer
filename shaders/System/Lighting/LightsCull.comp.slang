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
                for (uint cascade = 0; cascade < SwLighting::Shadow::VIEWS_PER_DIRECTIONAL; cascade++) {
                    uint view = SwLighting::Shadow::DIRECTIONAL_VIEW_BASE + shadowIndex * SwLighting::Shadow::VIEWS_PER_DIRECTIONAL + cascade;
                    lightsCullPc.mShadowsViewsBuffer[view].mLightIndex = lightIndex;
                    lightsCullPc.mShadowsViewsBuffer[view].mFace = cascade;
                }
                break;
            case SwLight::Type::Point:
                if (shadowIndex >= SwLighting::Shadow::MAX_POINT_SHADOW_MAPS) break;
                for (uint face = 0; face < SwLighting::Shadow::VIEWS_PER_POINT; face++) {
                    uint view = SwLighting::Shadow::POINT_VIEW_BASE + shadowIndex * SwLighting::Shadow::VIEWS_PER_POINT + face;
                    lightsCullPc.mShadowsViewsBuffer[view].mLightIndex = lightIndex;
                    lightsCullPc.mShadowsViewsBuffer[view].mFace = face;
                }
                break;
            case SwLight::Type::Spot: {
                if (shadowIndex >= SwLighting::Shadow::MAX_SPOT_SHADOW_MAPS) break;
                uint view = SwLighting::Shadow::SPOT_VIEW_BASE + shadowIndex;
                lightsCullPc.mShadowsViewsBuffer[view].mLightIndex = lightIndex;
                lightsCullPc.mShadowsViewsBuffer[view].mFace = 0;
                break;
            }
        }
    }
}
