module SwCommon;

namespace Sw {
    public static const uint MAX_1D_WORKGROUP_THREADS = 1024;
    public static const uint MAX_2D_WORKGROUP_THREADS = 32;
    public static const float PI = 3.1415926535897932384626433832795028841972f;
    public static const uint NUM_PBR_IMAGES = 5;
    public static const uint MAX_SHADOW_LIGHTS = 16;   // punctual lights selected per frame for shading + shadow casting
    public static const uint SHADOW_MAP_SIZE = 1024;   // mirrors C++ SwLighting::SHADOW_MAP_WIDTH_HEIGHT for PCF texel offsets

    public struct Perspective {
        public float4x4 mView;
        public float4x4 mProj;
    };

    public struct Sunlight {
        public float mIntensity;
        public float2 mAzimuthElevation;   
        public float3 mColor;

        public float3 getDirection() {
            float cosElev = cos(mAzimuthElevation.y);
            return float3(cos(mAzimuthElevation.x) * cosElev, sin(mAzimuthElevation.y), sin(mAzimuthElevation.x) * cosElev);
        }
    };

    public enum LightType : uint {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    public struct Light {
        public float3 mColor;
        public float mIntensity;
        public LightType mType;                 
        public float mRange;               // Less than or equal to 0 = infinite
        public float mInnerCos;            // spot inner cone, cos
        public float mOuterCos;            // spot outer cone, cos
        public uint mNodeTransformIndex;   // into the scene node-transform buffer
        public uint mInstanceIndex;        // into the scene instance buffer
        public float2 mPad;
    };

    public struct PerFrameData {
        public Perspective mPerspective;
        public Sunlight mSunlight;
        public float3 mCameraWorldPos;
        public uint mActiveLightCount;                     
        public uint mActiveLightIndices[MAX_SHADOW_LIGHTS]; 
        public float4x4 mLightViewProj[MAX_SHADOW_LIGHTS];  
    };

    public struct Vertex {
        public float3 mPosition;
        public float3 mNormal;
        public float4 mColor;
        public float2 mUv;
        public float4 mTangent; // w for handedness
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
}
