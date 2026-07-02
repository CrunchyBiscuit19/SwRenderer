module SwMesh;

namespace SwMesh {
    public static const uint NUM_AABB_CORNERS = 8;

    public struct Vertex {
        public float3 mPosition;
        public float3 mNormal;
        public float4 mColor;
        public float2 mUv;
        public float4 mTangent;  // w for handedness
    };

    public struct Bounds {
        public float3 mMin;
        public float3 mMax;
    };
}
