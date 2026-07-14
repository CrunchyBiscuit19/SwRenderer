module SwPick;

__exported import Data.SwBatch;
__exported import Data.SwCamera;
__exported import Data.SwFrame;
__exported import Data.SwInstance;
__exported import Data.SwMaterial;
__exported import Data.SwMesh;
__exported import Data.SwNode;

namespace SwPick {
    public struct DrawPC {
        public SwMesh::Vertex* mSceneVertexBuffer;
        public SwMaterial::Constant* mSceneMaterialConstantsBuffer;
        public SwNode::Transform* mSceneNodeTransformsBuffer;
        public SwInstance::Data* mSceneInstancesBuffer;
        public uint* mSceneRisIndicesBuffer;
        public SwBatch::RenderCommand *mSceneRcsBuffer;
        public SwFrame::Data *mFrameBuffer;
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mRenderItemIndex : SV_VulkanInstanceID;
        public uint mDrawIndex : SV_DrawIndex;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
        public float2 mUv : TEXCOORD0;
        public float4 mColor : COLOR0;
        public nointerpolation float mAlphaCutoff;
        public nointerpolation uint mMaterialIndex;
        public nointerpolation uint mAssetIndex;
        public nointerpolation uint mInstanceIndex;
    };

    public typedef VSOutput FSInput;

    public struct VSOutputOpaque {
        public float4 mPosition : SV_Position;
        public nointerpolation uint mAssetIndex;
        public nointerpolation uint mInstanceIndex;
    };

    public typedef VSOutputOpaque FSInputOpaque;

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
