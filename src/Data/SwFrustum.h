#pragma once

#include <glm/glm.hpp>

struct SwPlane {
    glm::vec3 mNormal;
    float mDistance;

    SwPlane() : mNormal(glm::vec3(0.f)), mDistance(0.f) {}
    SwPlane(glm::vec3 n, glm::vec3 p) : mNormal(glm::normalize(n)), mDistance(glm::dot(glm::normalize(n), p)) {}
};

struct SwFrustum {
    struct Data {
        SwPlane mNear;
        SwPlane mFar;
        SwPlane mLeft;
        SwPlane mRight;
        SwPlane mTop;
        SwPlane mBottom;
    };

    static Data calculateFrustum(
        const glm::vec3& position, const glm::vec3& forward, const glm::vec3& right, const glm::vec3& up, float halfHSide, float halfVSide, float nearPlane,
        float farPlane
    );
};
