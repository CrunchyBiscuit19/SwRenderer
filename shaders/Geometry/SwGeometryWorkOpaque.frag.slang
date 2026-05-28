import SwGeometry;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

SwGeometry::FSOutputOpaque main(SwGeometry::FSInput in) : SV_Target {
    SwGeometry::FSOutputOpaque output;

    float3 normal = normalize(in.mNormal);
    float3 sunlightDir = normalize(float3(1.f, 0.f, 0.f));
    float sunlightPower = 1.f;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.mMaterialIndex * 5 + 0)].Sample(in.mUv);
    if (diffuseColor.a < 0.0001f)
        discard;

    float4 color = in.mColor * diffuseColor;
    float finalAlpha = color.a;
    float diffuse = max(dot(normal, sunlightDir), 0.f);
    float4 sunlight = float4(float3(1.f), 1.f) * sunlightPower;

    float4 lightColor = color * diffuse * sunlight;
    float4 ambient = color * float4(float3(1.f), 1.f);

    float4 finalColor = lightColor + ambient;
    finalColor.a = finalAlpha;

    output.mColor = finalColor;

    return output;
}

// TODO un-hardcode sunlight data