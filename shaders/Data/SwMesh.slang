module SwMesh;

public struct SwVertex {
    public float3 mPosition;
    public float3 mNormal;
    public float4 mColor;
    public float2 mUv;
    public float4 mTangent;  // w for handedness
};

namespace SwMesh {
    public static const uint NUM_AABB_CORNERS = 8;
}

public struct SwBounds {
    public float3 mMin;
    public float3 mMax;

    // Collapses already-transformed corners back into an axis-aligned min/max pair.
    public static void fromCorners(float4 corners[SwMesh::NUM_AABB_CORNERS], out float3 aabbMin, out float3 aabbMax) {
        aabbMin = corners[0].xyz;
        aabbMax = corners[0].xyz;
        for (uint i = 1; i < SwMesh::NUM_AABB_CORNERS; i++) {
            aabbMin = min(aabbMin, corners[i].xyz);
            aabbMax = max(aabbMax, corners[i].xyz);
        }
    }
};
