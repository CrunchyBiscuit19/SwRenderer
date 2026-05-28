module SwPicker;

import SwCommon;

namespace SwPick {
    public struct DrawPC {
        public SwVertex* mSceneVertexBuffer;
        public SwNodeTransform* mSceneNodeTransformsBuffer;
        public SwInstance* mSceneInstancesBuffer;
        public uint* mSceneVisibleRenderInstancesInstanceIndexBuffer;
        public SwRenderItem *mPostCullRenderItemsBuffer;
        public SwPerFrameData *mPerFrameBuffer;
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mInstanceIndex : SV_VulkanInstanceID;
        public uint mDrawIndex : SV_DrawIndex;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
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
