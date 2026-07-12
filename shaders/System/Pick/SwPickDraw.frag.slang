import System.Pick.SwPick;

[[vk::binding(0, 0)]]
public uniform SamplerState materialSamplers[];
[[vk::binding(0, 1)]]
public uniform Texture2D materialTextures[];

[shader("fragment")]
[earlydepthstencil]
uint2 mainOpaque(SwPick::FSInputOpaque in) : SV_Target {
    return uint2(in.mAssetIndex + 1, in.mInstanceIndex + 1);
}

[shader("fragment")]
uint2 mainMasked(SwPick::FSInput in) : SV_Target {
    uint baseIdx = NonUniformResourceIndex(in.mMaterialIndex * SwMaterial::NUM_PBR_IMAGES + 0);
    float4 diffuseColor = materialTextures[baseIdx].Sample(materialSamplers[baseIdx], in.mUv);
    if (in.mColor.a * diffuseColor.a < in.mAlphaCutoff)
        discard;
    return uint2(in.mAssetIndex + 1, in.mInstanceIndex + 1);
}
