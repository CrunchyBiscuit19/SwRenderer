module SwBatch;

import Data.SwMaterial;

namespace SwBatch {
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
        public SwMaterial::Type mMaterialType;

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
            this.mMaterialType = src.mMaterialType;
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
}
