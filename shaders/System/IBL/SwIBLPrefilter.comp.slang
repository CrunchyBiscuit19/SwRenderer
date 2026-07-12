import Data.SwConstants;
import System.BRDF.SwBRDF;
import System.IBL.SwIBL;

// GGX-importance-sampled specular prefilter for all mips of the equirect prefilter chain. 
[[vk::binding(0, 0)]]
public uniform SamplerState environmentSampler;
[[vk::binding(1, 0)]]
public uniform Texture2D environmentMap;
[[vk::binding(2, 0)]]
public uniform RWTexture2D<float4> prefilterMips[SwIBL::MAX_PREFILTER_MIP_LEVELS];

[[vk::push_constant]]
SwIBL::PrefilterPC prefilterPc;

[numthreads(SwConstants::MAX_3D_WORKGROUP_THREADS, SwConstants::MAX_3D_WORKGROUP_THREADS, SwConstants::MAX_3D_WORKGROUP_THREADS)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID) {
    uint2 pixel;  
    pixel.x = dispatchThreadID.x % prefilterPc.mBaseExtent.x;
    pixel.y = dispatchThreadID.y % prefilterPc.mBaseExtent.y;
    uint mipLevel = dispatchThreadID.z;
    float roughness = (prefilterPc.mTotalMipLevels > 1) ? (float)mipLevel / (float)(prefilterPc.mTotalMipLevels - 1) : 0.f;

    if (mipLevel >= prefilterPc.mTotalMipLevels)
        return;

    uint2 extent;
    prefilterMips[mipLevel].GetDimensions(extent.x, extent.y);
    if (any(pixel >= extent))
        return;

    float2 uv = (float2(pixel) + 0.5f) / float2(extent);
    float3 N = normalize(SwBRDF::equirectUvToDir(uv));  // Split-sum approximation assumes the view, normal and reflection directions coincide.
    float3 V = N;

    // Solid angle covered by one texel of the source environment (4*PI sterads).
    uint2 envExtent;
    environmentMap.GetDimensions(envExtent.x, envExtent.y);
    float saTexel = 4.f * SwConstants::PI / float(envExtent.x * envExtent.y);

    float3 prefiltered = float3(0.f);
    float totalWeight = 0.f;
    for (uint i = 0u; i < SwIBL::PREFILTER_SAMPLE_COUNT; i++) {
        float2 xi = SwIBL::hammersley(i, SwIBL::PREFILTER_SAMPLE_COUNT);
        float3 H = SwIBL::importanceSampleGGX(xi, N, roughness);
        float3 L = normalize(2.f * dot(V, H) * H - V);
        float NdotL = dot(N, L);
        if (NdotL > 0.f) {
            // Sample the environment from the mip whose texel solid angle matches this sample's, so concentrated bright sources are pre-averaged on rough mips.
            float NdotH = max(dot(N, H), 0.f);
            float VdotH = max(dot(V, H), 1e-4f);
            float pdf = SwBRDF::distributionGGX(NdotH, roughness) * NdotH / (4.f * VdotH) + 1e-4f;
            float saSample = 1.f / (float(SwIBL::PREFILTER_SAMPLE_COUNT) * pdf);
            float mip = roughness <= 0.f ? 0.f : 0.5f * log2(saSample / saTexel);
            prefiltered += environmentMap.SampleLevel(environmentSampler, SwBRDF::dirToEquirectUv(L), max(mip, 0.f)).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefiltered = prefiltered / max(totalWeight, 1e-4f);
    prefilterMips[mipLevel][pixel] = float4(prefiltered, 1.f);
}
