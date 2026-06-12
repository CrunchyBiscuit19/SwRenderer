import SwGeometry;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

// Set 1: image-based lighting maps baked by SwIBL (irradiance, specular prefilter, BRDF LUT).
[[vk::binding(0, 1)]]
public uniform Sampler2D iblIrradianceMap;
[[vk::binding(1, 1)]]
public uniform Sampler2D iblPrefilterMap;
[[vk::binding(2, 1)]]
public uniform Sampler2D iblBrdfLut;

[[vk::push_constant]]
SwGeometry::WorkPC workPc;

SwGeometry::FSOutputTransparent main(SwGeometry::FSInput in) : SV_Target {
    SwGeometry::FSOutputTransparent output;

    Sw::MaterialConstant mc = workPc.mSceneMaterialConstantsBuffer[in.mMaterialIndex];
    uint base = in.mMaterialIndex * Sw::NUM_PBR_IMAGES;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(base + SwGeometry::BASE_TEXTURE_OFFSET)].Sample(in.mUv);
    float4 baseColor = in.mColor * diffuseColor;

    // glTF metallic-roughness texture: B = metallic, G = roughness; occlusion in R.
    float4 mrSample = materialResources[NonUniformResourceIndex(base + SwGeometry::METALLIC_ROUGHNESS_TEXTURE_OFFSET)].Sample(in.mUv);
    float metallic = mrSample.b * mc.mMetallicRoughnessFactor.x;
    float roughness = mrSample.g * mc.mMetallicRoughnessFactor.y;
    float ao = lerp(1.f, materialResources[NonUniformResourceIndex(base + SwGeometry::OCCLUSION_TEXTURE_OFFSET)].Sample(in.mUv).r, mc.mOcclusionStrength);

    float3 sampledNormal = materialResources[NonUniformResourceIndex(base + SwGeometry::NORMAL_TEXTURE_OFFSET)].Sample(in.mUv).rgb;
    float3 normal = SwGeometry::applyNormalMap(in.mNormal, in.mTangent, sampledNormal, mc.mNormalScale);

    float3 view = normalize(workPc.mPerFrameBuffer->mCameraWorldPos - in.mWorldPos);

    float3 litColor = SwGeometry::shadeLit(
        baseColor.rgb, metallic, roughness, ao, in.mWorldPos, normal, view, workPc.mPerFrameBuffer->mSunlight, workPc.mSceneLightsBuffer,
        workPc.mLightCount, workPc.mSceneInstancesBuffer, workPc.mSceneNodeTransformsBuffer, iblIrradianceMap, iblPrefilterMap, iblBrdfLut,
        workPc.mMaxPrefilterMip, workPc.mIblIntensity
    );
    litColor += materialResources[NonUniformResourceIndex(base + SwGeometry::EMISSIVE_TEXTURE_OFFSET)].Sample(in.mUv).rgb * mc.mEmissiveFactor.rgb;
    float4 finalColor = float4(litColor, baseColor.a);

    float depth = in.mPosition.z;
    float weight = clamp(
        pow(min(1.0, finalColor.a * 10.0) + 0.01, 3.0) * 1e3 * 
        pow(1.0 - depth * 0.9, 3.0),
        1e-2, 3e3
    );

    output.mAccum = float4(finalColor.rgb * finalColor.a, finalColor.a) * weight;
    output.mRvl = float4(finalColor.a * (1.0 - finalColor.rgb * 0.5), finalColor.a);

    return output;
}
