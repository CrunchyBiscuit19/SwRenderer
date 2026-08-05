import System.Geometry.SwGeometry;

[[vk::binding(0, 0)]]
public uniform SamplerState materialSamplers[];
[[vk::binding(0, 1)]]
public uniform Texture2D materialTextures[];

[[vk::push_constant]]
SwLighting::ShadowDrawPC shadowsDrawPc;

// Alpha-cutout discard so masked geometry casts holes.
[shader("fragment")]
void main(SwLighting::VSOutput in) {
    SwMaterial::Constant mc = shadowsDrawPc.mMaterialConstantsBuffer[in.mMaterialIndex];
    uint base = in.mMaterialIndex * SwMaterial::NUM_PBR_IMAGES;
    uint baseIdx = NonUniformResourceIndex(base + SwGeometry::BASE_TEXTURE_OFFSET);
    float4 diffuse = materialTextures[baseIdx].Sample(materialSamplers[baseIdx], in.mUv);
    if (in.mColor.a * diffuse.a < mc.mAlphaCutoff) {
        discard;
    }
}
