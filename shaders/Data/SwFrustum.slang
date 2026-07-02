module SwFrustum;

namespace SwFrustum {
    public static const uint NUM_FRUSTUM_PLANES = 6;

    public struct Plane {
        public float3 mNormal;
        public float mDistance;
    };

    public struct Data {
        public Plane mNear;
        public Plane mFar;
        public Plane mLeft;
        public Plane mRight;
        public Plane mTop;
        public Plane mBottom;
    };
}

