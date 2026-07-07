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
        public SwBatch::RenderCommand* mRcsBuffer;
        public uint mRcsLimit;
    };

    public enum Phase { Early, Late };

    public struct WorkPC {
        public SwBatch::RenderCommand* mRcsBuffer;
        public SwBatch::RenderItem* mRisBuffer;
        public uint* mRisCount;
        public SwFrame::Data* mFrameBuffer;
        public SwMesh::Bounds* mSceneBoundsBuffer;
        public SwNode::Transform* mSceneNodeTransformsBuffer;
        public SwInstance::Data* mSceneInstancesBuffer;
        public uint* mSceneDrawRisIndicesBuffer;
        public bool* mSceneVisibilityRisReadBuffer;
        public bool* mSceneVisibilityRisWriteBuffer;
        public uint mRisLimit;
        public Phase mPhase;
        public bool mHasEarlyDraw;
    };

    public struct CompactPC {
        public SwBatch::RenderCommand* mPreRcsBuffer;
        public SwBatch::RenderCommand* mPostRcsBuffer;
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

