import System.Geometry.SwGeometry;
import System.Lighting.SwLighting;

[[vk::binding(0, 0)]]
public uniform SamplerState materialSamplers[];
[[vk::binding(0, 1)]]
public uniform Texture2D materialTextures[];

[[vk::binding(0, 2)]]
public uniform SamplerState iblIrradianceSampler;
[[vk::binding(1, 2)]]
public uniform SamplerState iblPrefilterSampler;
[[vk::binding(2, 2)]]
public uniform SamplerState iblBrdfLutSampler;
[[vk::binding(3, 2)]]
public uniform Texture2D iblIrradianceMap;
[[vk::binding(4, 2)]]
public uniform Texture2D iblPrefilterMap;
[[vk::binding(5, 2)]]
public uniform Texture2D iblBrdfLut;

[[vk::binding(0, 3)]]
public uniform Texture2D shadow2DMaps[SwLighting::MAX_SHADOW_LIGHTS];
[[vk::binding(1, 3)]]
public uniform TextureCube shadowCubeMaps[SwLighting::MAX_SHADOW_CUBE_LIGHTS];
[[vk::binding(2, 3)]]
public uniform SamplerComparisonState shadowSampler;

[[vk::push_constant]]
SwGeometry::DrawPC drawPc;

SwGeometry::FSOutputTransparent main(SwGeometry::FSInput in) : SV_Target {
    SwGeometry::FSOutputTransparent output;

    SwMaterial::Constant mc = drawPc.mSceneMaterialConstantsBuffer[in.mMaterialIndex];
    uint base = in.mMaterialIndex * SwMaterial::NUM_PBR_IMAGES;

    uint baseIdx = NonUniformResourceIndex(base + SwGeometry::BASE_TEXTURE_OFFSET);
    float4 diffuseColor = materialTextures[baseIdx].Sample(materialSamplers[baseIdx], in.mUv);
    float4 baseColor = in.mColor * diffuseColor;

    // glTF metallic-roughness texture: B = metallic, G = roughness; occlusion in R.
    uint mrIdx = NonUniformResourceIndex(base + SwGeometry::METALLIC_ROUGHNESS_TEXTURE_OFFSET);
    float4 mrSample = materialTextures[mrIdx].Sample(materialSamplers[mrIdx], in.mUv);
    float metallic = mrSample.b * mc.mMetallicRoughnessFactor.x;
    float roughness = mrSample.g * mc.mMetallicRoughnessFactor.y;
    uint occlusionIdx = NonUniformResourceIndex(base + SwGeometry::OCCLUSION_TEXTURE_OFFSET);
    float ao = lerp(1.f, materialTextures[occlusionIdx].Sample(materialSamplers[occlusionIdx], in.mUv).r, mc.mOcclusionStrength);

    uint normalIdx = NonUniformResourceIndex(base + SwGeometry::NORMAL_TEXTURE_OFFSET);
    float3 sampledNormal = materialTextures[normalIdx].Sample(materialSamplers[normalIdx], in.mUv).rgb;
    float3 normal = SwGeometry::applyNormalMap(in.mNormal, in.mTangent, sampledNormal, mc.mNormalScale);

    float3 view = normalize(drawPc.mFrameBuffer->mCameraBuffer->mWorldPos - in.mWorldPos);

    float3 litColor = SwLighting::shadeLit(
        baseColor.rgb, metallic, roughness, ao, in.mWorldPos, normal, view, drawPc.mVisibleLightsBuffer, drawPc.mSceneLightsBuffer,
        drawPc.mSceneInstancesBuffer, drawPc.mSceneNodeTransformsBuffer, iblIrradianceMap, iblIrradianceSampler, iblPrefilterMap, iblPrefilterSampler,
        iblBrdfLut, iblBrdfLutSampler, drawPc.mMaxPrefilterMipLevel, drawPc.mIblIntensity, drawPc.mIblComponents, shadow2DMaps, shadowCubeMaps, shadowSampler
    );
    uint emissiveIdx = NonUniformResourceIndex(base + SwGeometry::EMISSIVE_TEXTURE_OFFSET);
    litColor += materialTextures[emissiveIdx].Sample(materialSamplers[emissiveIdx], in.mUv).rgb * mc.mEmissiveFactor.rgb;
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
