module SwMesh;

public struct SwVertex {
    public float3 mPosition;
    public float3 mNormal;
    public float4 mColor;
    public float2 mUv;
    public float4 mTangent;  // w for handedness
};

public struct SwBounds {
    public float3 mMin;
    public float3 mMax;
};

namespace SwMesh {
    public static const uint NUM_AABB_CORNERS = 8;
}
