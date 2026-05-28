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
};

public typedef float4x4 SwNodeTransform;

public struct SwRenderItem {
    public uint mIndexCount;
    public uint mRenderInstanceCount;
    public uint mFirstIndex;
    public uint mVertexOffset;
    public uint mFirstRenderInstance;
    public uint mMaterialIndex;
    public uint mNodeTransformIndex;
    public uint mModelIndex;
    public uint mFirstInstance;
    public uint mBoundsIndex;
};

public struct SwRenderInstance {
    public uint mRenderItemsIndex;
    public uint mSceneInstanceIndex;
}

/*
public float3 mAmbientColor;
public float3 mSunlightDirection;
public float mSunlightIntensity;
public float3 mSunlightColor;
*/