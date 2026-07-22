module SwCull;

__exported import Data.SwBatch;
__exported import Data.SwCamera;
__exported import Data.SwFrame;
__exported import Data.SwInstance;
__exported import Data.SwMesh;
__exported import Data.SwNode;
__exported import Data.SwFrustum;

namespace SwCull {
    public static const bool ENABLE_FRUSTUM_CULL = true;
    public static const bool ENABLE_OCCLUSION_CULL = true;

    public struct ResetPC {
        public SwRenderCommand* mRcsBuffer;
        public uint mRcsLimit;
    };

    public enum Phase { Early, Late };

    public struct TestPC {
        public SwRenderCommand* mRcsBuffer;
        public SwRenderItem* mRisBuffer;
        public SwBatch::Data* mBatchesBuffer;
        public uint* mRisCount;
        public SwFrame::Data* mFrameBuffer;
        public SwBounds* mBoundsBuffer;
        public SwNode::Transform* mNodeTransformsBuffer;
        public SwInstance::Data* mInstancesBuffer;
        public uint* mRisIndicesBuffer;
        public bool* mRisVisibilityReadBuffer;
        public bool* mRisVisibilityWriteBuffer;
        public uint mRisLimit;
        public Phase mPhase;
    };

    public struct CompactPC {
        public SwBatch::Data* mBatchesBuffer;
        public SwRenderCommand* mPreRcsBuffer;
        public SwRenderCommand* mPostRcsBuffer;
        public uint* mPostRcsCount;
        public uint mPreRcsLimit;
    };

    public struct PrepOcclusionPC {
        public int mLevel;
    };

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };
}

