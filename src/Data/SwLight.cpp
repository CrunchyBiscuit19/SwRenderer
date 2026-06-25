#include <Data/SwLight.h>

std::uint32_t SwLight::sLatestLightId{0};
SwStagingBuffer SwLight::sLightsStaging{};

SwLight::SwLight(Params params) : mId(sLatestLightId++), mParams(std::move(params)) {}

SwLight::Data SwLight::toData(std::uint32_t nodeTransformIndex, std::uint32_t instanceIndex) const {
    Data d;
    d.mType = static_cast<std::uint32_t>(mParams.mType);
    d.mColor = mParams.mColor;
    d.mIntensity = mParams.mIntensity;
    d.mRange = mParams.mRange;
    d.mInnerCos = std::cos(mParams.mInnerConeAngle);
    d.mOuterCos = std::cos(mParams.mOuterConeAngle);
    d.mNodeTransformIndex = nodeTransformIndex;
    d.mInstanceIndex = instanceIndex;
    return d;
}

void SwLight::init() { sLightsStaging = SwBufferFactory::createStagingBuffer("LightsStagingBuffer", LIGHTS_STAGING_BUFFER_SIZE); }

void SwLight::cleanup() { sLightsStaging.destroy(); }

static SwLight::Params makeDirectionalParams() {
    SwLight::Params params;
    params.mType = SwLight::Type::Directional;
    params.mColor = glm::vec3(1.f, 0.96f, 0.88f);
    params.mIntensity = 2.f;
    return params;
}

static SwLight::Params makeSpotParams() {
    SwLight::Params params;
    params.mType = SwLight::Type::Spot;
    params.mColor = glm::vec3(0.6f, 0.85f, 1.f);
    params.mIntensity = 30.f;
    params.mRange = 20.f;
    params.mInnerConeAngle = glm::radians(15.f);
    params.mOuterConeAngle = glm::radians(25.f);
    return params;
}

static SwLight::Params makePointParams() {
    SwLight::Params params;
    params.mType = SwLight::Type::Point;
    params.mColor = glm::vec3(1.f, 0.7f, 0.4f);
    params.mIntensity = 30.f;
    params.mRange = 20.f;
    return params;
}

SwTestDirectionalLight::SwTestDirectionalLight() : SwLight(makeDirectionalParams()) {}

SwTestSpotLight::SwTestSpotLight() : SwLight(makeSpotParams()) {}

SwTestPointLight::SwTestPointLight() : SwLight(makePointParams()) {}
