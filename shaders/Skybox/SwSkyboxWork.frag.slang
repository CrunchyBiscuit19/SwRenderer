import SwSkybox;

[[vk::binding(0, 0)]]
public uniform Sampler2D hdrMap;

static const float PI = 3.14159265359;

float4 main(SwSkybox::FSInput in) : SV_Target {
    float3 dir = normalize(in.mUvw);
    float phi = atan2(dir.z, dir.x);
    float theta = asin(dir.y);
    float2 uv = float2(phi / (2.0 * PI) + 0.5, theta / PI + 0.5);
    return hdrMap.Sample(uv);
}
