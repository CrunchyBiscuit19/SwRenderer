#pragma once

#include <Resource/SwBuffer.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cstdint>

class SwLight {
public:
    enum class Type : std::uint32_t { Directional = 0, Point = 1, Spot = 2 };

    struct Data {
        glm::vec3 mColor{1.f};
        float mIntensity{1.f};
        std::uint32_t mType{static_cast<std::uint32_t>(Type::Point)};
        float mRange{-1.f};
        float mInnerCos{1.f};
        float mOuterCos{-1.f};
        std::uint32_t mNodeTransformIndex{0};  // into mSceneNodeTransformsBuffer
        std::uint32_t mInstanceIndex{0};       // into mSceneInstancesBuffer
        glm::vec2 mPad{0.f};
    };

    struct Params {
        Type mType{Type::Point};
        glm::vec3 mColor{1.f};
        float mIntensity{1.f};
        float mRange{-1.f};                                 
        float mInnerConeAngle{0.f};                         // spot only, rads
        float mOuterConeAngle{glm::quarter_pi<float>()};    // spot only, rads
    };

private:
    static std::uint32_t sLatestLightId;

    std::uint32_t mId{0};
    Params mParams;

public:
    static constexpr std::uint32_t LIGHTS_STAGING_BUFFER_SIZE{1 << 16};  
    static SwStagingBuffer sLightsStaging;

    SwLight() = default;
    explicit SwLight(Params params);

    inline Params& getParams() { return mParams; }
    inline std::uint32_t getId() const { return mId; }

    Data toData(std::uint32_t nodeTransformIndex, std::uint32_t instanceIndex) const;

    static void init();
    static void cleanup();
};

struct SwSunlight {
    glm::vec3 mAmbient{0.03f};
    float mIntensity{1.f};
    glm::vec2 mAzimuthElevation{0.f};  
    glm::vec3 mColor{1.f};
};
