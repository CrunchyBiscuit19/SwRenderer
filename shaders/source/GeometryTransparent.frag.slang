import Geometry;
import Perspective;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(0, 1)]]
public uniform Sampler2D materialResources[];

FSOutputTransparent main(FSInput in) : SV_Target {
    FSOutputTransparent output;

    float3 normal = normalize(in.normal);
    float3 sunlightDir = normalize(perspectiveBuffer.sunlightDirection.xyz);
    float sunlightPower = perspectiveBuffer.sunlightDirection.w;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.materialIndex * 5 + 0)].Sample(in.uv);
    if (diffuseColor.a < 0.0001f)
        discard;

    float4 baseColor = in.color * diffuseColor;

    float diffuse = max(dot(normal, sunlightDir), 0.f);
    float3 sunlightContrib = perspectiveBuffer.sunlightColor * sunlightPower * diffuse;
    float3 ambientContrib = perspectiveBuffer.ambientColor;

    float3 litColor = baseColor.rgb * (sunlightContrib + ambientContrib);
    float4 finalColor = float4(litColor, baseColor.a);

    float depth = in.position.z;
    float weight = clamp(
        pow(min(1.0, finalColor.a * 10.0) + 0.01, 3.0) * 1e3 * 
        pow(1.0 - depth * 0.9, 3.0),
        1e-2, 3e3
    );

    output.accum = float4(finalColor.rgb * finalColor.a, finalColor.a) * weight;
    output.rvl = float4(finalColor.a * (1.0 - finalColor.rgb * 0.5), finalColor.a);

    return output;
}
