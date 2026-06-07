module SwCull;

import SwCommon;

namespace SwCull {
    public static const uint MAX_1D_WORKGROUP_THREADS = 1024;
    public static const uint MAX_2D_WORKGROUP_THREADS = 32;
    public static const uint NUM_FRUSTUM_PLANES = 6;
    public static const uint NUM_AABB_CORNERS = 8;

    public struct Plane {
        public float3 mNormal;
        public float mDistance;
    }

    public struct ResetPC {
        public SwRenderItem* mRItemsBuffer;
        public uint mRItemsLimit;
    };

    public enum Phase { Early, Late };

    public struct WorkPC {
        public SwRenderItem* mRItemsBuffer;
        public SwRenderInstance* mRInstsBuffer;
        public uint* mRInstsCount;
        public Plane* mFrustumBuffer;
        public SwPerFrameData* mPerFrameBuffer;
        public SwBounds* mSceneBoundsBuffer;
        public SwNodeTransform* mSceneNodeTransformsBuffer;
        public SwInstance* mSceneInstancesBuffer;
        public uint* mSceneVisibleRInstsIndicesBuffer;
        public uint* mSceneVisibilityRInstsReadBuffer;
        public uint* mSceneVisibilityRInstsWriteBuffer;
        public uint mRInstsLimit;
        public float2 mDrawExtents;
        public uint2 mDepthPyramidExtents;
        public Phase mPhase;
    };

    public struct CompactPC {
        public SwRenderItem* mPreRItemsBuffer;
        public SwRenderItem* mPostRItemsBuffer;
        public uint* mPostRItemsCount;
        public uint mPreRItemsLimit;
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

