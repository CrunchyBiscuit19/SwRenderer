import SwGeometry;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

[[vk::push_constant]]
SwGeometry::WorkPC workPc;

float4 shade(SwGeometry::FSInput in, bool masked) {
    Sw::MaterialConstant mc = workPc.mSceneMaterialConstantsBuffer[in.mMaterialIndex];
    uint base = in.mMaterialIndex * Sw::NUM_PBR_IMAGES;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(base + SwGeometry::BASE_TEXTURE_OFFSET)].Sample(in.mUv);
    if (masked) {
        if (in.mColor.a * diffuseColor.a < mc.mAlphaCutoff)
            discard;
    }

    float4 color = in.mColor * diffuseColor;

    // glTF metallic-roughness texture: B = metallic, G = roughness; occlusion in R.
    float4 mrSample = materialResources[NonUniformResourceIndex(base + SwGeometry::METALLIC_ROUGHNESS_TEXTURE_OFFSET)].Sample(in.mUv);
    float metallic = mrSample.b * mc.mMetallicRoughnessFactor.x;
    float roughness = mrSample.g * mc.mMetallicRoughnessFactor.y;
    float ao = lerp(1.f, materialResources[NonUniformResourceIndex(base + SwGeometry::OCCLUSION_TEXTURE_OFFSET)].Sample(in.mUv).r, mc.mOcclusionStrength);

    float3 sampledNormal = materialResources[NonUniformResourceIndex(base + SwGeometry::NORMAL_TEXTURE_OFFSET)].Sample(in.mUv).rgb;
    float3 normal = SwGeometry::applyNormalMap(in.mNormal, in.mTangent, sampledNormal, mc.mNormalScale);

    float3 view = normalize(workPc.mPerFrameBuffer->mCameraWorldPos - in.mWorldPos);

    float3 lit = SwGeometry::shadeLit(
        color.rgb, metallic, roughness, ao, in.mWorldPos, normal, view, workPc.mPerFrameBuffer->mSunlight, workPc.mSceneLightsBuffer,
        workPc.mLightCount, workPc.mSceneInstancesBuffer, workPc.mSceneNodeTransformsBuffer
    );
    lit += materialResources[NonUniformResourceIndex(base + SwGeometry::EMISSIVE_TEXTURE_OFFSET)].Sample(in.mUv).rgb * mc.mEmissiveFactor.rgb;

    return float4(lit, color.a);
}

// Opaque geometry: no discard, so early depth test/write is safe.
[shader("fragment")]
[earlydepthstencil]
SwGeometry::FSOutputOpaque mainOpaque(SwGeometry::FSInput in) : SV_Target {
    SwGeometry::FSOutputOpaque output;
    output.mColor = shade(in, false);
    return output;
}

// Masked geometry: alpha-cutout discard, so depth must not be written early.
[shader("fragment")]
SwGeometry::FSOutputOpaque mainMasked(SwGeometry::FSInput in) : SV_Target {
    SwGeometry::FSOutputOpaque output;
    output.mColor = shade(in, true);
    return output;
}