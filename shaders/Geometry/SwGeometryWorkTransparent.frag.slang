import SwGeometry;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

SwGeometry::FSOutputTransparent main(SwGeometry::FSInput in) : SV_Target {
    SwGeometry::FSOutputTransparent output;

    float3 normal = normalize(in.mNormal);
    float3 sunlightDir = normalize(float3(1.f, 0.f, 0.f));
    float sunlightPower = 1.f;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.mMaterialIndex * Sw::NUM_PBR_IMAGES + 0)].Sample(in.mUv);
    if (diffuseColor.a < 0.0001f)
        discard;

    float4 baseColor = in.mColor * diffuseColor;

    float diffuse = max(dot(normal, sunlightDir), 0.f);
    float3 sunlightContrib = float3(1.f) * sunlightPower * diffuse;
    float3 ambientContrib = float3(1.f);

    float3 litColor = baseColor.rgb * (sunlightContrib + ambientContrib);
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
