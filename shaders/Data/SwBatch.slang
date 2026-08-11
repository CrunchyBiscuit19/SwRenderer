module SwBatch;

import Data.SwMaterial;

public struct SwRenderCommand {
    public uint mIndexCount;
    public uint mRiCount;
    public uint mFirstIndex;
    public uint mVertexOffset;
    public uint mFirstRi;
    public uint mMaterialIndex;
    public uint mNodeTransformIndex;
    public uint mAssetIndex;
    public uint mFirstInstance;
    public uint mBoundsIndex;
    public uint mBatchIndex;
    public uint mShadowView;

    [mutating]
    public void copyFrom(SwRenderCommand* src) {
        this.mIndexCount = src->mIndexCount;
        this.mRiCount = src->mRiCount;
        this.mFirstIndex = src->mFirstIndex;
        this.mVertexOffset = src->mVertexOffset;
        this.mFirstRi = src->mFirstRi;
        this.mMaterialIndex = src->mMaterialIndex;
        this.mNodeTransformIndex = src->mNodeTransformIndex;
        this.mAssetIndex = src->mAssetIndex;
        this.mFirstInstance = src->mFirstInstance;
        this.mBoundsIndex = src->mBoundsIndex;
        this.mBatchIndex = src->mBatchIndex;
        this.mShadowView = src->mShadowView;
    }
};

public struct SwRenderItem {
    public uint mRcIndex;
    public uint mInstanceIndex;

    [mutating]
    public void copyFrom(SwRenderItem* src) {
        this.mRcIndex = src->mRcIndex;
        this.mInstanceIndex = src->mInstanceIndex;
    }
}

namespace SwBatch {
    public struct Data {
        public SwMaterial::Type mMaterialType;
        public uint mRcsIndex;
        public uint mRcsSize;
        public uint mRisIndex;
        public uint mRisSize;
    };
}
