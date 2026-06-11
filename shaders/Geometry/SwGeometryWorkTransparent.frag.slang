import SwGeometry;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

[[vk::push_constant]]
SwGeometry::WorkPC workPc;

SwGeometry::FSOutputTransparent main(SwGeometry::FSInput in) : SV_Target {
    SwGeometry::FSOutputTransparent output;

    float3 normal = normalize(in.mNormal);

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.mMaterialIndex * Sw::NUM_PBR_IMAGES + 0)].Sample(in.mUv);

    float4 baseColor = in.mColor * diffuseColor;

    float3 litColor = SwGeometry::shadeLit(
        baseColor.rgb, in.mWorldPos, normal, workPc.mPerFrameBuffer->mSunlight, workPc.mSceneLightsBuffer, workPc.mLightCount, workPc.mSceneInstancesBuffer,
        workPc.mSceneNodeTransformsBuffer
    );
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
