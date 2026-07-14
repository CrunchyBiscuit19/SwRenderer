module SwCamera;

__exported import Data.SwFrustum;

namespace SwCamera {
    public struct Perspective {
        public float4x4 mView;
        public float4x4 mProj;
    };

    public struct Data {
        public Perspective mPerspective;
        public float3 mWorldPos;
        public SwFrustum mFrustum;
    };
}
