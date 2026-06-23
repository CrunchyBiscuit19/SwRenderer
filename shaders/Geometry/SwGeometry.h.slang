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

    // Build a world-space TBN, robust to missing / degenerate vertex tangents.
    public float3 applyNormalMap(float3 normalWS, float4 tangentWS, float3 sampledNormal, float normalScale) {
        float3 n = normalize(normalWS);

        // Gram-Schmidt re-orthonormalise the tangent against the normal. 
        // If the supplied tangent is zero, NaN, or (nearly) parallel to the normal, the residual collapses toward zero. Normalising this is bad (the "acne"). 
        // Detect that and derive a stable basis from the normal instead. The !(len > eps) test also treats a NaN residual as degenerate.
        float3 t = tangentWS.xyz - n * dot(n, tangentWS.xyz);
        float tLen = length(t);
        float handedness = tangentWS.w;
        if (!(tLen > 1e-4f)) {
            float3 up = abs(n.y) < 0.999f ? float3(0.f, 1.f, 0.f) : float3(1.f, 0.f, 0.f);
            t = normalize(cross(up, n));
            handedness = 1.f;
        } else {
            t = t / tLen;
        }
        float3 b = cross(n, t) * handedness; // handedness from glTF tangent.w
        float3 ts = sampledNormal * 2.f - 1.f; // unpack [0,1] -> [-1,1]
        ts.xy *= normalScale;
        return normalize(t * ts.x + b * ts.y + n * ts.z);
    }

    public struct FSOutputOpaque {
        public float4 mColor : SV_Target0;
    };

    public struct FSOutputTransparent {
        public float4 mAccum : SV_Target0;
        public float4 mRvl : SV_Target1;
    };
};

