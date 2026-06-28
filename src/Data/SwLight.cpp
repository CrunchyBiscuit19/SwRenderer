#include <Data/SwLight.h>

std::uint32_t SwLight::sLatestLightId{0};
SwStagingBuffer SwLight::sLightsStaging{};

SwLight::SwLight() : mId(sLatestLightId++) {}

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
