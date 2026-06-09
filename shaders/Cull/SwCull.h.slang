module SwCull;

__exported import SwCommon;

namespace SwCull {
    public static const uint NUM_FRUSTUM_PLANES = 6;
    public static const uint NUM_AABB_CORNERS = 8;

    public static const bool ENABLE_FRUSTUM_CULL = true;
    public static const bool ENABLE_OCCLUSION_CULL = true;

    public struct Plane {
        public float3 mNormal;
        public float mDistance;
    }

    public struct ResetPC {
        public Sw::RenderCommand* mRcsBuffer;
        public uint mRcsLimit;
    };

    public enum Phase { Early, Late };

    public struct WorkPC {
        public Sw::RenderCommand* mRcsBuffer;
        public Sw::RenderItem* mRisBuffer;
        public uint* mRisCount;
        public Plane* mFrustumBuffer;
        public Sw::PerFrameData* mPerFrameBuffer;
        public Sw::Bounds* mSceneBoundsBuffer;
        public Sw::NodeTransform* mSceneNodeTransformsBuffer;
        public Sw::Instance* mSceneInstancesBuffer;
        public uint* mSceneDrawRisIndicesBuffer;
        public bool* mSceneVisibilityRisReadBuffer;
        public bool* mSceneVisibilityRisWriteBuffer;
        public uint mRisLimit;
        public float2 mDrawExtents;
        public uint2 mDepthPyramidExtents;
        public Phase mPhase;
        public bool mHasEarlyDraw;
    };

    public struct CompactPC {
        public Sw::RenderCommand* mPreRcsBuffer;
        public Sw::RenderCommand* mPostRcsBuffer;
        public uint* mPostRcsCount;
        public uint mPreRcsLimit;
    };

    public struct PrepOcclusionPC {
        public uint2 mDepthPyramidExtent;
        public uint2 mDepthFullExtent;
        public float2 mDepthFullRatio;
        public int mLevel;
    };

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };
}

