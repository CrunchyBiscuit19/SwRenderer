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

    // Solid angle covered by one texel of the source environment (equirect spans 4*PI steradians).
    uint2 envExtent;
    environmentMap.GetDimensions(envExtent.x, envExtent.y);
    float saTexel = 4.f * Sw::PI / float(envExtent.x * envExtent.y);

    float3 irradiance = float3(0.f);
    float numSamples = 0.f;
    const float sampleDelta = 0.025f;
    for (float phi = 0.f; phi < 2.f * Sw::PI; phi += sampleDelta) {
        for (float theta = 0.f; theta < 0.5f * Sw::PI; theta += sampleDelta) {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            // Each sample stands in for a sin(theta)*dTheta*dPhi patch of the hemisphere. Read the env from the
            // mip whose texel solid angle matches that patch so a sub-texel-sized sun is pre-averaged instead of
            // being point-sampled and aliasing into an over-bright diffuse term.
            float saSample = sin(theta) * sampleDelta * sampleDelta;
            float mip = 0.5f * log2(max(saSample, 1e-12f) / saTexel);
            // cos(theta) weights by incident angle, sin(theta) accounts for the hemisphere area element.
            irradiance += environmentMap.SampleLevel(SwBRDF::dirToEquirectUv(sampleVec), max(mip, 0.f)).rgb * cos(theta) * sin(theta);
            numSamples += 1.f;
        }
    }
    irradiance = Sw::PI * irradiance / max(numSamples, 1.f);
    irradianceMap[pixel] = float4(irradiance, 1.f);
}
