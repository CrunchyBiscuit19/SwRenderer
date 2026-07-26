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

    SwLight::Data light = lightsCullPc.mLightsBuffer[lightIndex];

    float3 center;
    float radius;
    bool visible = true;
    if (SwLighting::Collision::lightBoundingSphere(light, lightsCullPc.mInstancesBuffer, lightsCullPc.mNodeTransformsBuffer, center, radius)) {
        SwFrustum frustum = lightsCullPc.mFrameBuffer->mCameraBuffer->mFrustum;
        visible = sphereInFrustum(frustum, center, radius);
    }

    if (visible) {
        uint offset;
        InterlockedAdd(lightsCullPc.mLitIndicesBuffer[0], 1, offset);  
        lightsCullPc.mLitIndicesBuffer[1 + offset] = lightIndex;       
    }
}
