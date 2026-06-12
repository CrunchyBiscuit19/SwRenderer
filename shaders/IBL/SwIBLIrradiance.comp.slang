import SwCommon;
import SwBRDF;
import SwIBL;

// Convolves the environment equirect into a small cosine-weighted diffuse irradiance map.
// Binding 0: the source environment (combined image sampler). Binding 1: the irradiance output.
[[vk::binding(0, 0)]]
public uniform Sampler2D environmentMap;
[[vk::binding(1, 0)]]
public uniform RWTexture2D<float4> irradianceMap;

[numthreads(Sw::MAX_2D_WORKGROUP_THREADS, Sw::MAX_2D_WORKGROUP_THREADS, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID) {
    uint2 pixel = dispatchThreadID.xy;
    uint2 extent;
    irradianceMap.GetDimensions(extent.x, extent.y);
    if (any(pixel >= extent))
        return;

    float2 uv = (float2(pixel) + 0.5f) / float2(extent);
    float3 N = normalize(SwBRDF::equirectUvToDir(uv));

    // Tangent basis around N to walk the hemisphere.
    float3 up = abs(N.y) < 0.999f ? float3(0.f, 1.f, 0.f) : float3(1.f, 0.f, 0.f);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float3 irradiance = float3(0.f);
    float numSamples = 0.f;
    const float sampleDelta = 0.025f;
    for (float phi = 0.f; phi < 2.f * SwIBL::PI; phi += sampleDelta) {
        for (float theta = 0.f; theta < 0.5f * SwIBL::PI; theta += sampleDelta) {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            // cos(theta) weights by incident angle, sin(theta) accounts for the hemisphere area element.
            irradiance += environmentMap.SampleLevel(SwBRDF::dirToEquirectUv(sampleVec), 0.f).rgb * cos(theta) * sin(theta);
            numSamples += 1.f;
        }
    }
    irradiance = SwIBL::PI * irradiance / max(numSamples, 1.f);
    irradianceMap[pixel] = float4(irradiance, 1.f);
}
