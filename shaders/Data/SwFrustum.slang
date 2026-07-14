module SwFrustum;

public struct SwPlane {
    public float3 mNormal;
    public float mDistance;
};

public struct SwFrustum {
    public SwPlane mNear;
    public SwPlane mFar;
    public SwPlane mLeft;
    public SwPlane mRight;
    public SwPlane mTop;
    public SwPlane mBottom;

    public static const uint NUM_FRUSTUM_PLANES = 6;

    // Orthonormal basis perpendicular to forward, matching the camera convention (right = forward x worldUp).
    public static void basisFromForward(float3 forward, out float3 right, out float3 up) {
        up = (abs(forward.y) < 0.999f) ? float3(0.f, 1.f, 0.f) : float3(1.f, 0.f, 0.f);
        right = normalize(cross(forward, up));
        up = cross(right, forward);
    }

    // Perspective frustum with inward-pointing plane normals. The four side planes pass through position (the apex).
    public static SwFrustum calculateFrustum(float3 position, float3 forward, float3 right, float3 up, float halfHSide, float halfVSide, float near, float far) {
        SwFrustum f;
        f.mNear.mNormal = forward;
        f.mNear.mDistance = dot(forward, position + forward * near);
        f.mFar.mNormal = -forward;
        f.mFar.mDistance = dot(-forward, position + forward * far);
        f.mLeft.mNormal = normalize(cross(forward - right * halfHSide, up));
        f.mLeft.mDistance = dot(f.mLeft.mNormal, position);
        f.mRight.mNormal = normalize(cross(up, forward + right * halfHSide));
        f.mRight.mDistance = dot(f.mRight.mNormal, position);
        f.mTop.mNormal = normalize(cross(forward + up * halfVSide, right));
        f.mTop.mDistance = dot(f.mTop.mNormal, position);
        f.mBottom.mNormal = normalize(cross(right, forward - up * halfVSide));
        f.mBottom.mDistance = dot(f.mBottom.mNormal, position);
        return f;
    }
};
