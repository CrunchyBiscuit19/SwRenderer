import SwCommon;
import SwIBL;

// Pre-integrates the environment-BRDF scale/bias for the split-sum specular term. Output is a
// 2D LUT over (NdotV = u, roughness = v); environment-independent, so baked once at startup.
[[vk::binding(0, 0)]]
public uniform RWTexture2D<float2> brdfLut;

static const uint SAMPLE_COUNT = 1024u;

// Schlick-GGX geometry with the IBL k = roughness^2 / 2 (distinct from the direct-lighting k).
float geometrySchlickGGX(float NdotX, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.f;
    return NdotX / (NdotX * (1.f - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

[numthreads(Sw::MAX_2D_WORKGROUP_THREADS, Sw::MAX_2D_WORKGROUP_THREADS, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID) {
    uint2 pixel = dispatchThreadID.xy;
    uint2 extent;
    brdfLut.GetDimensions(extent.x, extent.y);
    if (any(pixel >= extent))
        return;

    float2 uv = (float2(pixel) + 0.5f) / float2(extent);
    float NdotV = max(uv.x, 1e-4f);
    float roughness = uv.y;

    float3 V = float3(sqrt(1.f - NdotV * NdotV), 0.f, NdotV);
    float3 N = float3(0.f, 0.f, 1.f);

    float a = 0.f;
    float b = 0.f;
    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        float2 xi = SwIBL::hammersley(i, SAMPLE_COUNT);
        float3 H = SwIBL::importanceSampleGGX(xi, N, roughness);
        float3 L = normalize(2.f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.f);
        float NdotH = max(H.z, 0.f);
        float VdotH = max(dot(V, H), 0.f);
        if (NdotL > 0.f) {
            float G = geometrySmith(NdotV, NdotL, roughness);
            float gVis = (G * VdotH) / max(NdotH * NdotV, 1e-4f);
            float fc = pow(1.f - VdotH, 5.f);
            a += (1.f - fc) * gVis;
            b += fc * gVis;
        }
    }
    brdfLut[pixel] = float2(a, b) / float(SAMPLE_COUNT);
}
