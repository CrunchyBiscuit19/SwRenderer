import SwCommon;
import SwBRDF;
import SwIBL;

// GGX-importance-sampled specular prefilter for one roughness level (one mip of the equirect
// prefilter chain). Dispatched once per mip with mRoughness = mip / (mipCount - 1).
[[vk::binding(0, 0)]]
public uniform Sampler2D environmentMap;
[[vk::binding(1, 0)]]
public uniform RWTexture2D<float4> prefilterMip;

[[vk::push_constant]]
SwIBL::PrefilterPC pc;

static const uint SAMPLE_COUNT = 1024u;

[numthreads(Sw::MAX_2D_WORKGROUP_THREADS, Sw::MAX_2D_WORKGROUP_THREADS, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID) {
    uint2 pixel = dispatchThreadID.xy;
    uint2 extent;
    prefilterMip.GetDimensions(extent.x, extent.y);
    if (any(pixel >= extent))
        return;

    float2 uv = (float2(pixel) + 0.5f) / float2(extent);
    // Split-sum approximation assumes the view, normal and reflection directions coincide.
    float3 N = normalize(SwBRDF::equirectUvToDir(uv));
    float3 V = N;

    // Solid angle covered by one texel of the source environment (equirect spans 4*PI steradians).
    uint2 envExtent;
    environmentMap.GetDimensions(envExtent.x, envExtent.y);
    float saTexel = 4.f * Sw::PI / float(envExtent.x * envExtent.y);

    float3 prefiltered = float3(0.f);
    float totalWeight = 0.f;
    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        float2 xi = SwIBL::hammersley(i, SAMPLE_COUNT);
        float3 H = SwIBL::importanceSampleGGX(xi, N, pc.mRoughness);
        float3 L = normalize(2.f * dot(V, H) * H - V);
        float NdotL = dot(N, L);
        if (NdotL > 0.f) {
            // Sample the environment from the mip whose texel solid angle matches this sample's, so
            // concentrated bright sources are pre-averaged instead of producing fireflies on rough mips.
            float NdotH = max(dot(N, H), 0.f);
            float VdotH = max(dot(V, H), 1e-4f);
            float pdf = SwBRDF::distributionGGX(NdotH, pc.mRoughness) * NdotH / (4.f * VdotH) + 1e-4f;
            float saSample = 1.f / (float(SAMPLE_COUNT) * pdf);
            float mip = pc.mRoughness <= 0.f ? 0.f : 0.5f * log2(saSample / saTexel);
            prefiltered += environmentMap.SampleLevel(SwBRDF::dirToEquirectUv(L), max(mip, 0.f)).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefiltered = prefiltered / max(totalWeight, 1e-4f);
    prefilterMip[pixel] = float4(prefiltered, 1.f);
}
