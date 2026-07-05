#include <Data/SwLight.h>

std::uint32_t SwLight::sLatestLightId{0};

SwLight::SwLight() : mId(sLatestLightId++) {}

SwLight::SwLight(Params params) : mId(sLatestLightId++), mParams(std::move(params)) {}

glm::vec3 SwLight::worldPosition(const glm::mat4& instanceTransform, const glm::mat4& nodeWorldTransform) const {
    return glm::vec3(instanceTransform * nodeWorldTransform * glm::vec4(mBasePosition, 1.f));
}

glm::vec3 SwLight::worldDirection(const glm::mat4& instanceTransform, const glm::mat4& nodeWorldTransform) const {
    return glm::normalize(glm::vec3(instanceTransform * nodeWorldTransform * glm::vec4(mBaseDirection, 0.f)));
}

SwLight::Data SwLight::toData() const {
    Data d;
    d.mType = static_cast<std::uint32_t>(mParams.mType);
    d.mColor = mParams.mColor;
    d.mIntensity = mParams.mIntensity;
    d.mRange = mParams.mRange;
    d.mInnerCos = std::cos(mParams.mInnerConeAngle);
    d.mOuterCos = std::cos(mParams.mOuterConeAngle);
    d.mNodeTransformIndex = mNodeTransformIndex;
    d.mInstanceIndex = mInstanceIndex;
    d.mBasePosition = mBasePosition;
    d.mBaseDirection = mBaseDirection;
    return d;
}

void SwLight::init() {}

void SwLight::cleanup() {}
