import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::LightsCullPC lightsCullPc;

bool sphereInFrustum(SwFrustum frustum, float3 center, float radius) {
    SwPlane planes[SwFrustum::NUM_FRUSTUM_PLANES] = {
        frustum.mNear, frustum.mFar, frustum.mLeft, frustum.mRight, frustum.mTop, frustum.mBottom
    };
    for (uint i = 0; i < SwFrustum::NUM_FRUSTUM_PLANES; i++) {
        if (dot(planes[i].mNormal, center) - planes[i].mDistance < -radius) {
            return false;
        }
    }
    return true;
}

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint lightIndex = in.mDispatchThreadID.x;
    if (lightIndex >= lightsCullPc.mLightsLimit) {
        return;
    }

    SwLight::Data light = lightsCullPc.mSceneLightsBuffer[lightIndex];

    bool visible = true;
    if (light.mType != SwLight::Type::Directional && light.mRange > 0.f) {
        float4x4 model = mul(
            lightsCullPc.mSceneInstancesBuffer[light.mInstanceIndex].mTransformMatrix, lightsCullPc.mSceneNodeTransformsBuffer[light.mNodeTransformIndex]
        );
        float3 center = mul(model, float4(light.mBasePosition, 1.f)).xyz;
        float3 forward = normalize(mul(model, float4(light.mBaseDirection, 0.f)).xyz);
        float radius = light.mRange;

        if (light.mType == SwLight::Type::Spot) {
            float cosHalf = light.mOuterCos;
            if (cosHalf * cosHalf >= 0.5f) {  
                radius = light.mRange / (2.f * cosHalf * cosHalf);
                center += forward * radius;
            } else {  
                float sinHalf = sqrt(1.f - cosHalf * cosHalf);
                radius = light.mRange * sinHalf / cosHalf;
                center += forward * light.mRange;
            }
        }

        SwFrustum frustum = lightsCullPc.mFrameBuffer->mCameraBuffer->mFrustum;
        visible = sphereInFrustum(frustum, center, radius);
    }

    if (visible) {
        uint offset;
        InterlockedAdd(lightsCullPc.mLitIndicesBuffer[0], 1, offset);  
        lightsCullPc.mLitIndicesBuffer[1 + offset] = lightIndex;       
    }
}
