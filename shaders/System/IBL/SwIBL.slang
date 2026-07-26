module SwIBL;

__exported import Data.SwCamera;
__exported import Data.SwFrame;
__exported import Data.SwConstants;
import System.BRDF.SwBRDF;

namespace SwIBL {
    public static const uint DIFFUSE = 1u;
    public static const uint SPECULAR = 2u;

    public static const uint MAX_PREFILTER_MIP_LEVELS = 1 << 4;

    public static const uint PREFILTER_SAMPLE_COUNT = 1024u;

    public float radicalInverseVdC(uint bits) {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return float(bits) * 2.3283064365386963e-10f;  // / 2^32
    }

    public float2 hammersley(uint i, uint n) { return float2(float(i) / float(n), radicalInverseVdC(i)); }

    public float3 importanceSampleGGX(float2 xi, float3 N, float roughness) {
        float a = roughness * roughness;
        float phi = 2.f * SwConstants::PI * xi.x;
        float cosTheta = sqrt((1.f - xi.y) / (1.f + (a * a - 1.f) * xi.y));
        float sinTheta = sqrt(max(1.f - cosTheta * cosTheta, 0.f));

        float3 h = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

        float3 up = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
        float3 tangent = normalize(cross(up, N));
        float3 bitangent = cross(N, tangent);
        return normalize(tangent * h.x + bitangent * h.y + N * h.z);
    }
    
    public struct Params {
        public Texture2D mIrradianceMap;
        public SamplerState mIrradianceSampler;
        public Texture2D mPrefilterMap;
        public SamplerState mPrefilterSampler;
        public Texture2D mBrdfLut;
        public SamplerState mBrdfLutSampler;
        public float mMaxPrefilterMip;
        public float mIntensity;
        public uint mComponents;
    };

    public float3 ambientIBL(float3 albedo, float metallic, float roughness, float ao, float3 normal, float3 view, Params params) {
        float NdotV = max(dot(normal, view), 1e-4f);
        float3 F0 = lerp(float3(0.04f), albedo, metallic);

        float3 ambient = float3(0.f);
        if ((params.mComponents & DIFFUSE) != 0u) {
            float3 kD = (1.f - F0) * (1.f - metallic);
            float3 irradiance = params.mIrradianceMap.SampleLevel(params.mIrradianceSampler, SwBRDF::dirToEquirectUv(normal), 0.f).rgb;
            ambient += kD * irradiance * albedo;
        }
        if ((params.mComponents & SPECULAR) != 0u) {
            float3 R = reflect(-view, normal);
            float prefilterLod = roughness * params.mMaxPrefilterMip;
            float3 prefiltered = params.mPrefilterMap.SampleLevel(params.mPrefilterSampler, SwBRDF::dirToEquirectUv(R), prefilterLod).rgb;
            float2 envBrdf = params.mBrdfLut.SampleLevel(params.mBrdfLutSampler, float2(NdotV, roughness), 0.f).rg;
            ambient += prefiltered * (F0 * envBrdf.x + envBrdf.y);
        }

        return ambient * ao;
    }

    public struct PrefilterPC {
        public uint mTotalMipLevels;
        public uint2 mBaseExtent;
    }

    public struct SkyboxPC {
        public SwIBL::Vertex* mSkyboxVertexBuffer;
        public SwFrame::Data* mFrameBuffer;
    };

    public struct Vertex {
        public float4 mPosition;
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
        public float3 mUvw : TEXCOORD0;
    };

    public typedef VSOutput FSInput;
}
