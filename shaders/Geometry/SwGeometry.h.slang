module SwGeometry;

import SwCommon;

namespace SwGeometry {
    public static const uint NUM_PBR_IMAGES = 5;

    public struct WorkPC {
        public SwVertex* mSceneVertexBuffer;
        public SwMaterialConstant* mSceneMaterialConstantsBuffer;
        public SwNodeTransform* mSceneNodeTransformsBuffer;
        public SwInstance* mSceneInstancesBuffer;
        public uint* mSceneVisibleRInstsIndicesBuffer;
        public SwRenderItem *mDrawRItemsBuffer;
        public SwPerFrameData *mPerFrameBuffer;
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mInstanceIndex : SV_VulkanInstanceID;
        public uint mDrawIndex : SV_DrawIndex;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
        public float3 mNormal : NORMAL;
        public float2 mUv : TEXCOORD0;
        public float4 mColor : COLOR0;
        public nointerpolation uint mMaterialIndex;
    };

    public typedef VSOutput FSInput;

    public struct FSOutputOpaque {
        public float4 mColor : SV_Target0;
    };

    public struct FSOutputTransparent {
        public float4 mAccum : SV_Target0;
        public float4 mRvl : SV_Target1;
    };
};

