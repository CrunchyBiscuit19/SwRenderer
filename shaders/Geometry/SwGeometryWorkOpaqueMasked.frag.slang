import SwGeometry;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

[[vk::push_constant]]
SwGeometry::WorkPC workPc;

float4 shade(SwGeometry::FSInput in, bool masked) {
    float3 normal = normalize(in.mNormal);

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.mMaterialIndex * Sw::NUM_PBR_IMAGES + 0)].Sample(in.mUv);
    if (masked) {
        float alphaCutoff = workPc.mSceneMaterialConstantsBuffer[in.mMaterialIndex].mAlphaCutoff;
        if (in.mColor.a * diffuseColor.a < alphaCutoff)
            discard;
    }

    float4 color = in.mColor * diffuseColor;
    float3 lit = SwGeometry::shadeLit(
        color.rgb, in.mWorldPos, normal, workPc.mPerFrameBuffer->mSunlight, workPc.mSceneLightsBuffer, workPc.mLightCount, workPc.mSceneInstancesBuffer,
        workPc.mSceneNodeTransformsBuffer
    );

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