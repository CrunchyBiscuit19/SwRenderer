module SwCommon;

public struct SwPerspective {
    public float4x4 mView;
    public float4x4 mProj;
};

public struct SwPerFrameData {
    public SwPerspective mPerspective;
};

public struct SwVertex {
    public float3 mPosition;
    public float3 mNormal;
    public float4 mColor;
    public float2 mUv;
};

public struct SwBounds {
    public float3 mMin;
    public float3 mMax;
};

public struct SwInstance {
    public float4x4 mTransformMatrix;
};

public struct SwMaterialConstant {
    public float4 mBaseFactor;
    public float4 mEmissiveFactor;
    public float2 mMetallicRoughnessFactor;
    public float mNormalScale;
    public float mOcclusionStrength;
    public float mAlphaCutoff;
};

public typedef float4x4 SwNodeTransform;

public struct SwRenderCommand {
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
    public void copyFrom(SwRenderCommand* src) {
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

public struct SwRenderItem {
    public uint mRcIndex;
    public uint mSceneInstanceIndex;

    [mutating]
    public void copyFrom(SwRenderItem* src) {
        this.mRcIndex = src.mRcIndex;
        this.mSceneInstanceIndex = src.mSceneInstanceIndex;
    }
}

public struct SwSunlight {
    public float3 mAmbient;
    public float3 mDirection;
    public float mIntensity;
    public float3 mColor;
};
