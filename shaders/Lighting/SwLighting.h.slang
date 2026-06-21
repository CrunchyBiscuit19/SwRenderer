module SwLighting;

import SwCommon;
import SwCull;

namespace SwLighting {
    public struct ShadowCullPC {
        public Sw::RenderCommand* mLightRcsBuffer;
        public Sw::RenderItem* mRisBuffer;
        public uint mRisCount;
        public uint* mLightDrawRisIndicesBuffer;
        public SwCull::Plane* mFrustumBuffer;
        public Sw::PerFrameData* mPerFrameBuffer;
        public Sw::Bounds* mSceneBoundsBuffer;
        public Sw::NodeTransform* mSceneNodeTransformsBuffer;
        public Sw::Instance* mSceneInstancesBuffer;
    };

    public struct ShadowDrawPC {
        public uint* mLightDrawRisIndicesBuffer;
        public Sw::RenderCommand* mLightRcsBuffer;
        public Sw::PerFrameData* mPerFrameBuffer;
        public Sw::Vertex* mSceneVertexBuffer;
        public Sw::NodeTransform* mSceneNodeTransformsBuffer;
        public Sw::Instance* mSceneInstancesBuffer;
        public Sw::MaterialConstant* mSceneMaterialConstantsBuffer;
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mInstanceIndex : SV_VulkanInstanceID;
        public uint mDrawIndex : SV_DrawIndex;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
        public float2 mUv : TEXCOORD0;
        public float4 mColor : COLOR0;
        public nointerpolation float mAlphaCutoff;
        public nointerpolation uint mMaterialIndex;
        public nointerpolation uint mModelIndex;
        public nointerpolation uint mInstanceIndex;
    };

    public struct VSOutputOpaque {
        public float4 mPosition : SV_Position;
        public nointerpolation uint mModelIndex;
        public nointerpolation uint mInstanceIndex;
    };

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };
}