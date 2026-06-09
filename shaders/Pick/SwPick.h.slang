module SwPicker;

import SwCommon;

namespace SwPick {
    public struct DrawPC {
        public Sw::Vertex* mSceneVertexBuffer;
        public Sw::MaterialConstant* mSceneMaterialConstantsBuffer;
        public Sw::NodeTransform* mSceneNodeTransformsBuffer;
        public Sw::Instance* mSceneInstancesBuffer;
        public uint* mSceneDrawRisIndicesBuffer;
        public Sw::RenderCommand *mDrawRcsBuffer;
        public Sw::PerFrameData *mPerFrameBuffer;
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

    public typedef VSOutput FSInput;

    public struct ReadbackData {
        public int2 mCoords;
        public uint2 mRead;
    };
    public struct ReadbackPC {
        public ReadbackData* mReadbackBuffer;
    };

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };
}
