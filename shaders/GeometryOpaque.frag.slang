import Geometry;
import Perspective;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(0, 1)]]
public uniform Sampler2D materialResources[];

FSOutputOpaque main(FSInput in) : SV_Target {
    FSOutputOpaque output;

    float3 normal = normalize(in.normal);
    float3 sunlightDir = normalize(perspectiveBuffer.sunlightDirection.xyz);
    float sunlightPower = perspectiveBuffer.sunlightDirection.w;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.materialIndex * 5 + 0)].Sample(in.uv);
    if (diffuseColor.a < 0.0001f)
        discard;

    float4 color = in.color * diffuseColor;
    float finalAlpha = color.a;
    float diffuse = max(dot(normal, sunlightDir), 0.f);
    float4 sunlight = float4(perspectiveBuffer.sunlightColor, 1.f) * sunlightPower;

    float4 lightColor = color * diffuse * sunlight;
    float4 ambient = color * float4(perspectiveBuffer.ambientColor, 1.f);

    float4 finalColor = lightColor + ambient;
    finalColor.a = finalAlpha;

    output.color = finalColor;

    return output;
}
