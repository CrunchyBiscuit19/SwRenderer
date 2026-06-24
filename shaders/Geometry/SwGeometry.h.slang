module SwGeometry;

import SwCommon;
import SwBRDF;

namespace SwGeometry {
    public static const uint BASE_TEXTURE_OFFSET = 0;
    public static const uint METALLIC_ROUGHNESS_TEXTURE_OFFSET = 1;
    public static const uint NORMAL_TEXTURE_OFFSET = 2;
    public static const uint OCCLUSION_TEXTURE_OFFSET = 3;
    public static const uint EMISSIVE_TEXTURE_OFFSET = 4;

    public struct WorkPC {
        public Sw::Vertex* mSceneVertexBuffer;
        public Sw::MaterialConstant* mSceneMaterialConstantsBuffer;
        public Sw::NodeTransform* mSceneNodeTransformsBuffer;
        public Sw::Instance* mSceneInstancesBuffer;
        public uint* mSceneDrawRisIndicesBuffer;
        public Sw::RenderCommand* mDrawRcsBuffer;
        public Sw::PerFrameData* mPerFrameBuffer;
        public Sw::Light* mSceneLightsBuffer;
        public float mMaxPrefilterMip;   // highest mip index of the specular prefilter chain
        public float mIblIntensity;      // scales the image-based ambient term (GUI-controlled)
        public uint mIblComponents;      // IBL_DIFFUSE / IBL_SPECULAR bit mask (GUI-controlled)
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mInstanceIndex : SV_VulkanInstanceID;
        public uint mDrawIndex : SV_DrawIndex;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
        public float3 mWorldPos : POSITION0;
        public float3 mNormal : NORMAL;
        public float4 mTangent : TANGENT; // w = handedness
        public float2 mUv : TEXCOORD0;
        public float4 mColor : COLOR0;
        public nointerpolation uint mMaterialIndex;
    };

    public typedef VSOutput FSInput;

    // World-space TBN.
    public float3 applyNormalMap(float3 worldNormal, float4 worldTangent, float3 sampledNormal, float normalScale) {
        float3 N = normalize(worldNormal);    
        float3 T = normalize(worldTangent.xyz);
        T = normalize(T - mul(N, dot(T, N)));
        float3 B = mul(worldTangent.w, cross(N, T)); // w for handedness
        float3x3 TBN = float3x3(T, B, N);
        sampledNormal = sampledNormal * 2.f - 1.f;
        sampledNormal.xy *= normalScale;
        return normalize(mul(sampledNormal, TBN)); 
    }

    public struct FSOutputOpaque {
        public float4 mColor : SV_Target0;
    };

    public struct FSOutputTransparent {
        public float4 mAccum : SV_Target0;
        public float4 mRvl : SV_Target1;
    };
};

