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
        public SwRenderItem* mPreCullRenderItemsBuffer;
        public uint mPreCullRenderItemsLimit;
    };

    public struct DepthPyramidPC {
        public uint2 mDepthPyramidExtent;
        public uint2 mDepthFullExtent;
        public float2 mDepthFullRatio;
        public uint mLevel;
        public bool mReadFromFull;
    };

    public struct WorkPC {
        public SwRenderItem* mPreCullRenderItemsBuffer;
        public SwRenderInstance* mRenderInstancesBuffer;
        public uint* mRenderInstancesCountBuffer;
        public Plane* mFrustumBuffer;
        public SwPerFrameData* mPerFrameBuffer;
        public SwBounds* mSceneBoundsBuffer;
        public SwNodeTransform* mSceneNodeTransformsBuffer;
        public SwInstance* mSceneInstancesBuffer;
        public uint* mSceneVisibleRenderInstancesInstanceIndexBuffer;
        public uint mRenderInstancesLimit;
        public float2 mDrawExtents;
        public uint2 mDepthPyramidExtents;
    };

    public struct CompactPC {
        public SwRenderItem* mPreCullRenderItemsBuffer;
        public SwRenderItem* mPostCullRenderItemsBuffer;
        public uint* mPostCullRenderItemsCountBuffer;
        public uint mPreCullRenderItemsLimit;
    };

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };
}

