import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::LightsFrustumPC lightsFrustumPc;

// (+X, -X, +Y, -Y, +Z, -Z)
static const float3 CUBE_FACE_FORWARDS[SwLighting::Shadow::VIEWS_PER_POINT] = {
    float3(1.f, 0.f, 0.f), float3(-1.f, 0.f, 0.f),
    float3(0.f, 1.f, 0.f), float3(0.f, -1.f, 0.f),
    float3(0.f, 0.f, 1.f), float3(0.f, 0.f, -1.f)
};
static const float3 CUBE_FACE_UPS[SwLighting::Shadow::VIEWS_PER_POINT] = {
    float3(0.f, -1.f, 0.f), float3(0.f, -1.f, 0.f),
    float3(0.f, 0.f, 1.f), float3(0.f, 0.f, -1.f),
    float3(0.f, -1.f, 0.f), float3(0.f, -1.f, 0.f)
};

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint viewIndex = in.mDispatchThreadID.x;
    if (viewIndex >= lightsFrustumPc.mShadowViewsLimit) {
        return;
    }

    SwLighting::Shadow::View view = lightsFrustumPc.mShadowsViewsBuffer[viewIndex];
    if (view.mLightIndex == 0xFFFFFFFFu) {
        return;
    }

    SwLight::Data light = lightsFrustumPc.mLightsBuffer[view.mLightIndex];

    float4x4 model = mul(
        lightsFrustumPc.mInstancesBuffer[light.mInstanceIndex].mTransformMatrix,
        lightsFrustumPc.mNodeTransformsBuffer[light.mNodeTransformIndex]
    );
    float3 apex = mul(model, float4(light.mBasePosition, 1.f)).xyz;
    float far = (light.mRange > 0.f) ? light.mRange : SwLighting::Shadow::SHADOWS_SPOT_DEFAULT_RANGE;

    float3 forward;
    float halfSide;
    float3 viewUp;  
    float3 right;
    switch (light.mType) {
    case SwLight::Type::Spot:
        forward = normalize(mul(model, float4(light.mBaseDirection, 0.f)).xyz);
        // The outer cone half angle sets the frustum half extent at unit forward distance.
        float sinOuter = sqrt(max(1.f - light.mOuterCos * light.mOuterCos, 0.f));
        halfSide = sinOuter / max(light.mOuterCos, 1e-4f);
        viewUp = float3(0.f);  
        SwFrustum::basisFromForward(forward, right, viewUp);
        break;
    case SwLight::Type::Point:
        forward = CUBE_FACE_FORWARDS[view.mFace];
        halfSide = 1.f;  // 90 degree field of view per cube face
        viewUp = CUBE_FACE_UPS[view.mFace];
        break;
    default:
        return;
    }

    lightsFrustumPc.mLightsFrustumsBuffer[viewIndex] =
        SwFrustum::calculateFrustum(apex, forward, right, viewUp, halfSide, halfSide, SwLighting::Shadow::SHADOWS_SPOT_NEAR, far);
    lightsFrustumPc.mShadowsViewsBuffer[viewIndex].mViewProj =
        SwLighting::Shadow::buildViewProj(apex, forward, viewUp, halfSide, SwLighting::Shadow::SHADOWS_SPOT_NEAR, far);
}
