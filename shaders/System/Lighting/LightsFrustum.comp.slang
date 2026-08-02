import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::LightsFrustumPC lightsFrustumPc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint lightIndex = in.mDispatchThreadID.x;
    if (lightIndex >= lightsFrustumPc.mLightsLimit) {
        return;
    }

    SwLight::Data light = lightsFrustumPc.mLightsBuffer[lightIndex];

    if (light.mType != SwLight::Type::Spot) {
        return;
    }

    float4x4 model = mul(
        lightsFrustumPc.mInstancesBuffer[light.mInstanceIndex].mTransformMatrix,
        lightsFrustumPc.mNodeTransformsBuffer[light.mNodeTransformIndex]
    );
    float3 apex = mul(model, float4(light.mBasePosition, 1.f)).xyz;
    float3 forward = normalize(mul(model, float4(light.mBaseDirection, 0.f)).xyz);

    float3 right;
    float3 up;
    SwFrustum::basisFromForward(forward, right, up);

    // The outer cone half angle sets the frustum half extent at unit forward distance.
    float sinOuter = sqrt(max(1.f - light.mOuterCos * light.mOuterCos, 0.f));
    float halfSide = sinOuter / max(light.mOuterCos, 1e-4f);
    float far = (light.mRange > 0.f) ? light.mRange : SwLighting::Shadow::SHADOWS_SPOT_DEFAULT_RANGE;

    lightsFrustumPc.mLightsBuffer[lightIndex].mFrustum =
        SwFrustum::calculateFrustum(apex, forward, right, up, halfSide, halfSide, SwLighting::Shadow::SHADOWS_SPOT_NEAR, far);
}
