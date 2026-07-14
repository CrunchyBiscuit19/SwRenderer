module SwGeometry;

__exported import Data.SwBatch;
__exported import Data.SwCamera;
__exported import Data.SwFrame;
__exported import Data.SwInstance;
__exported import Data.SwLight;
__exported import System.Lighting.SwLighting;
__exported import Data.SwMaterial;
__exported import Data.SwMesh;
__exported import Data.SwNode;
import System.BRDF.SwBRDF;

namespace SwGeometry {
    public static const uint BASE_TEXTURE_OFFSET = 0;
    public static const uint METALLIC_ROUGHNESS_TEXTURE_OFFSET = 1;
    public static const uint NORMAL_TEXTURE_OFFSET = 2;
    public static const uint OCCLUSION_TEXTURE_OFFSET = 3;
    public static const uint EMISSIVE_TEXTURE_OFFSET = 4;

    public struct DrawPC {
        public SwMesh::Vertex* mSceneVertexBuffer;
        public SwMaterial::Constant* mSceneMaterialConstantsBuffer;
        public SwNode::Transform* mSceneNodeTransformsBuffer;
        public SwInstance::Data* mSceneInstancesBuffer;
        public uint* mSceneRisIndicesBuffer;
        public SwBatch::RenderCommand* mSceneRcsBuffer;
        public SwFrame::Data* mFrameBuffer;
        public SwLight::Data* mSceneLightsBuffer;
        public SwLighting::LightsInfo* mSceneLightsInfoBuffer;
        public float mMaxPrefilterMipLevel;
        public float mIblIntensity;
        public uint mIblComponents;
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mRenderItemIndex : SV_VulkanInstanceID;
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

        float3 t = T - N * dot(T, N);
        if (!(length(t) > 1e-4f)) {                                   
            float3 up = abs(N.y) < 0.999f ? float3(0,1,0) : float3(1,0,0);
            T = normalize(cross(up, N));                          
            worldTangent.w = 1.f;
        } else {
            T = normalize(t);
        }
        
        float3 B = worldTangent.w * cross(N, T); // w for handedness
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

