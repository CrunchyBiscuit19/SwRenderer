#include <Data/SwFrustum.h>

SwFrustum::Data SwFrustum::calculateFrustum(
    const glm::vec3& position, const glm::vec3& forward, const glm::vec3& right, const glm::vec3& up, float halfHSide, float halfVSide, float nearPlane,
    float farPlane
) {
    SwFrustum::Data frustum;
    frustum.mNear = SwPlane(forward, position + forward * nearPlane);
    frustum.mFar = SwPlane(-forward, position + forward * farPlane);
    frustum.mLeft = SwPlane(glm::cross(forward - right * halfHSide, up), position);
    frustum.mRight = SwPlane(glm::cross(up, forward + right * halfHSide), position);
    frustum.mTop = SwPlane(glm::cross(forward + up * halfVSide, right), position);
    frustum.mBottom = SwPlane(glm::cross(right, forward - up * halfVSide), position);
    // Cross product between slanted vectors and up / right vectors gives plane normals pointing inward.
    // Planes stretch indefinitely. Left, right, top, bottom planes all pass through the position. Near and far calculate with normal * distance.
    return frustum;
}
