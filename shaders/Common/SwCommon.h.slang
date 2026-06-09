module SwCommon;

namespace Sw {
    public static const uint MAX_1D_WORKGROUP_THREADS = 1024;
    public static const uint MAX_2D_WORKGROUP_THREADS = 32;

    public static const uint NUM_PBR_IMAGES = 5;

    public struct Perspective {
        public float4x4 mView;
        public float4x4 mProj;
    };

    public struct PerFrameData {
        public Perspective mPerspective;
    };

    public struct Vertex {
        public float3 mPosition;
        public float3 mNormal;
        public float4 mColor;
        public float2 mUv;
    };

    public struct Bounds {
        public float3 mMin;
        public float3 mMax;
    };

    public struct Instance {
        public float4x4 mTransformMatrix;
    };

    public struct MaterialConstant {
        public float4 mBaseFactor;
        public float4 mEmissiveFactor;
        public float2 mMetallicRoughnessFactor;
        public float mNormalScale;
        public float mOcclusionStrength;
        public float mAlphaCutoff;
    };

    public typedef float4x4 NodeTransform;

    public struct RenderCommand {
        public uint mIndexCount;
        public uint mRiCount;
        public uint mFirstIndex;
        public uint mVertexOffset;
        public uint mFirstRi;
        public uint mMaterialIndex;
        public uint mNodeTransformIndex;
        public uint mModelIndex;
        public uint mFirstInstance;
        public uint mBoundsIndex;

        [mutating]
        public void copyFrom(RenderCommand* src) {
            this.mIndexCount = src.mIndexCount;
            this.mRiCount = src.mRiCount;
            this.mFirstIndex = src.mFirstIndex;
            this.mVertexOffset = src.mVertexOffset;
            this.mFirstRi = src.mFirstRi;
            this.mMaterialIndex = src.mMaterialIndex;
            this.mNodeTransformIndex = src.mNodeTransformIndex;
            this.mModelIndex = src.mModelIndex;
            this.mFirstInstance = src.mFirstInstance;
            this.mBoundsIndex = src.mBoundsIndex;
        }
    };

    public struct RenderItem {
        public uint mRcIndex;
        public uint mSceneInstanceIndex;

        [mutating]
        public void copyFrom(RenderItem* src) {
            this.mRcIndex = src.mRcIndex;
            this.mSceneInstanceIndex = src.mSceneInstanceIndex;
        }
    }

    public struct Sunlight {
        public float3 mAmbient;
        public float3 mDirection;
        public float mIntensity;
        public float3 mColor;
    };
}
