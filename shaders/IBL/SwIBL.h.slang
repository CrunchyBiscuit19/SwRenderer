module SwIBL;

import SwCommon;

namespace SwIBL {
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
