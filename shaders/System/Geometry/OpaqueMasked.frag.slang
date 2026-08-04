import System.Geometry.SwGeometry;
import System.Lighting.SwLighting;
import System.IBL.SwIBL;

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
public uniform Texture2DArray directionalShadowMaps;
[[vk::binding(1, 3)]]
public uniform TextureCubeArray pointShadowMaps;
[[vk::binding(2, 3)]]
public uniform Texture2DArray spotShadowMaps;
[[vk::binding(3, 3)]]
public uniform SamplerComparisonState shadowsSampler;

[[vk::push_constant]]
SwGeometry::DrawPC drawPc;

float4 shade(SwGeometry::FSInput in, bool masked) {
    SwMaterial::Constant mc = drawPc.mMaterialConstantsBuffer[in.mMaterialIndex];
    uint base = in.mMaterialIndex * SwMaterial::NUM_PBR_IMAGES;

    uint baseIdx = NonUniformResourceIndex(base + SwGeometry::BASE_TEXTURE_OFFSET);
    float4 diffuseColor = materialTextures[baseIdx].Sample(materialSamplers[baseIdx], in.mUv);
    if (masked) {
        if (in.mColor.a * diffuseColor.a < mc.mAlphaCutoff)
            discard;
    }

    float4 color = in.mColor * diffuseColor;

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

    SwLighting::Calculation::SurfacePoint surface;
    surface.mAlbedo = color.rgb;
    surface.mMetallic = metallic;
    surface.mRoughness = roughness;
    surface.mAo = ao;
    surface.mWorldPos = in.mWorldPos;
    surface.mNormal = normal;
    surface.mView = view;

    SwIBL::Params ibl;
    ibl.mIrradianceMap = iblIrradianceMap;
    ibl.mIrradianceSampler = iblIrradianceSampler;
    ibl.mPrefilterMap = iblPrefilterMap;
    ibl.mPrefilterSampler = iblPrefilterSampler;
    ibl.mBrdfLut = iblBrdfLut;
    ibl.mBrdfLutSampler = iblBrdfLutSampler;
    ibl.mMaxPrefilterMip = drawPc.mMaxPrefilterMipLevel;
    ibl.mIntensity = drawPc.mIblIntensity;
    ibl.mComponents = drawPc.mIblComponents;

    SwLighting::Shadow::Params shadows;
    shadows.mDirectionalMaps = directionalShadowMaps;
    shadows.mPointMaps = pointShadowMaps;
    shadows.mSpotMaps = spotShadowMaps;
    shadows.mSampler = shadowsSampler;
    shadows.mViews = drawPc.mShadowsViewsBuffer;

    uint clusterIndex = SwLighting::getClusterIndex(
        in.mPosition.xy, in.mPosition.z, drawPc.mTargetSize, drawPc.mFrameBuffer->mCameraBuffer->mPlanes
    );

    SwLighting::Calculation::ClusterLights clusterLights;
    clusterLights.mIndicesBuffer = drawPc.mClustersLightIndicesBuffer;
    clusterLights.mCount = drawPc.mClustersLightCounts[clusterIndex];
    clusterLights.mOffset = drawPc.mClustersLightOffsetsBuffer[clusterIndex];

    float3 lit = SwLighting::Calculation::shadeLit(
        surface, clusterLights, drawPc.mLightsBuffer, drawPc.mInstancesBuffer, drawPc.mNodeTransformsBuffer, ibl, shadows
    );
    uint emissiveIdx = NonUniformResourceIndex(base + SwGeometry::EMISSIVE_TEXTURE_OFFSET);
    lit += materialTextures[emissiveIdx].Sample(materialSamplers[emissiveIdx], in.mUv).rgb * mc.mEmissiveFactor.rgb;

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

// Depth pre-pass masked geometry: alpha-cutout discard only, no color output.
[shader("fragment")]
void mainZPassMasked(SwGeometry::FSInput in) {
    SwMaterial::Constant* mc = &drawPc.mMaterialConstantsBuffer[in.mMaterialIndex];
    uint base = in.mMaterialIndex * SwMaterial::NUM_PBR_IMAGES;
    uint baseIdx = NonUniformResourceIndex(base + SwGeometry::BASE_TEXTURE_OFFSET);
    float4 diffuseColor = materialTextures[baseIdx].Sample(materialSamplers[baseIdx], in.mUv);
    if (in.mColor.a * diffuseColor.a < mc->mAlphaCutoff)
        discard;
}
