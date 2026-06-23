module SwIBL;

import SwCommon;
import SwBRDF;

namespace SwIBL {
    public static const uint IBL_DIFFUSE = 1u;
    public static const uint IBL_SPECULAR = 2u;

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
        float phi = 2.f * Sw::PI * xi.x;
        float cosTheta = sqrt((1.f - xi.y) / (1.f + (a * a - 1.f) * xi.y));
        float sinTheta = sqrt(max(1.f - cosTheta * cosTheta, 0.f));

        float3 h = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

        float3 up = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
        float3 tangent = normalize(cross(up, N));
        float3 bitangent = cross(N, tangent);
        return normalize(tangent * h.x + bitangent * h.y + N * h.z);
    }
    
    // Image-based ambient: split-sum diffuse irradiance + specular prefilter, modulated by AO.
    // Replaces the flat constant ambient now that the skybox drives a real environment.
    public float3 ambientIBL(
        float3 albedo, float metallic, float roughness, float ao, float3 normal, float3 view, Sampler2D irradianceMap, Sampler2D prefilterMap,
        Sampler2D brdfLut, float maxPrefilterMip, uint components
    ) {
        float NdotV = max(dot(normal, view), 1e-4f);
        float3 F0 = lerp(float3(0.04f), albedo, metallic);

        float3 ambient = float3(0.f);
        if ((components & IBL_DIFFUSE) != 0u) {
            // Weight by the normal-incidence Fresnel F0 (not the view-dependent F) so the diffuse fill
            // stays camera-invariant while keeping a sensible diffuse/specular energy split.
            float3 kD = (1.f - F0) * (1.f - metallic);
            float3 irradiance = irradianceMap.SampleLevel(SwBRDF::dirToEquirectUv(normal), 0.f).rgb;
            ambient += kD * irradiance * albedo;
        }
        if ((components & IBL_SPECULAR) != 0u) {
            float3 R = reflect(-view, normal);
            float prefilterLod = roughness * maxPrefilterMip;
            float3 prefiltered = prefilterMap.SampleLevel(SwBRDF::dirToEquirectUv(R), prefilterLod).rgb;
            float2 envBrdf = brdfLut.SampleLevel(float2(NdotV, roughness), 0.f).rg;
            ambient += prefiltered * (F0 * envBrdf.x + envBrdf.y);
        }

        return ambient * ao;
    }

    public struct PrefilterPC {
        public float mRoughness;
    };

    public struct SkyboxPC {
        public SwIBL::Vertex* mWorkVertexBuffer;
        public Sw::PerFrameData* mPerFrameBuffer;
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
