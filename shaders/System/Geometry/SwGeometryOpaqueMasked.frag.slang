import System.Geometry.SwGeometry;
import System.Lighting.SwLighting;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

[[vk::binding(0, 1)]]
public uniform Sampler2D iblIrradianceMap;
[[vk::binding(1, 1)]]
public uniform Sampler2D iblPrefilterMap;
[[vk::binding(2, 1)]]
public uniform Sampler2D iblBrdfLut;

[[vk::binding(0, 2)]]
public uniform Texture2D shadow2DMaps[SwLighting::MAX_SHADOW_LIGHTS];
[[vk::binding(1, 2)]]
public uniform TextureCube shadowCubeMaps[SwLighting::MAX_SHADOW_CUBE_LIGHTS];
[[vk::binding(2, 2)]]
public uniform SamplerComparisonState shadowSampler;

[[vk::push_constant]]
SwGeometry::DrawPC drawPc;

float4 shade(SwGeometry::FSInput in, bool masked) {
    SwMaterial::Constant mc = drawPc.mSceneMaterialConstantsBuffer[in.mMaterialIndex];
    uint base = in.mMaterialIndex * SwMaterial::NUM_PBR_IMAGES;

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

    float3 view = normalize(drawPc.mFrameBuffer->mCameraBuffer->mWorldPos - in.mWorldPos);

    float3 lit = SwLighting::shadeLit(
        color.rgb,
        metallic,
        roughness,
        ao,
        in.mWorldPos,
        normal,
        view,
        drawPc.mVisibleLightsBuffer,
        drawPc.mSceneLightsBuffer,
        drawPc.mSceneInstancesBuffer,
        drawPc.mSceneNodeTransformsBuffer,
        iblIrradianceMap,
        iblPrefilterMap,
        iblBrdfLut,
        drawPc.mMaxPrefilterMipLevel,
        drawPc.mIblIntensity,
        drawPc.mIblComponents,
        shadow2DMaps,
        shadowCubeMaps,
        shadowSampler
    );
    lit += materialResources[NonUniformResourceIndex(base + SwGeometry::EMISSIVE_TEXTURE_OFFSET)].Sample(in.mUv).rgb * mc.mEmissiveFactor.rgb;

    return float4(lit, 1.f);
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
