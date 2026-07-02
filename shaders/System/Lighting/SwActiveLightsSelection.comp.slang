import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ActiveLightsSelectionPC activeLightsSelectionPc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    // TODO implement selection: populate mActiveLightsBuffer->mCount and mActiveLightIndices.
    // If this passes some as-yet-unspecified selection test, it increments count, adds to mActiveLightsBuffer, and continues execution to calculate frustum.
    uint slot = in.mDispatchThreadID.x;

    SwLighting::ActiveLights* activeLights = activeLightsSelectionPc.mActiveLightsBuffer;
    if (slot >= activeLights->mCount) {
        return;
    }

    uint sceneLightIndex = activeLights->mActiveLightIndices[slot];
    SwLight::Data light = activeLightsSelectionPc.mSceneLightsBuffer[sceneLightIndex];
    if (light.mType != SwLight::Type::Spot) {
        return;
    }

    float4x4 model = mul(
        activeLightsSelectionPc.mSceneInstancesBuffer[light.mInstanceIndex].mTransformMatrix,
        activeLightsSelectionPc.mSceneNodeTransformsBuffer[light.mNodeTransformIndex]
    );
    float3 apex = mul(model, float4(light.mBasePosition, 1.f)).xyz;
    float3 forward = normalize(mul(model, float4(light.mBaseDirection, 0.f)).xyz);

    float3 right;
    float3 up;
    SwFrustum::basisFromForward(forward, right, up);

    float outerAngle = acos(clamp(light.mOuterCos, -0.9999f, 0.9999f));
    float halfSide = tan(outerAngle);
    float far = (light.mRange > 0.f) ? light.mRange : SwLighting::SHADOW_SPOT_DEFAULT_RANGE;

    activeLights->mActiveLightFrustums[slot] =
        SwFrustum::calculateFrustum(apex, forward, right, up, halfSide, halfSide, SwLighting::SHADOW_SPOT_NEAR, far);
}
