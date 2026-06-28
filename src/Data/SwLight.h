#pragma once

#include <Resource/SwBuffer.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cstdint>


class SwLight {
public:
    static constexpr std::uint32_t MAX_ACTIVE_LIGHTS{16};
    
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
        float mRange{5.f};                                 
        float mInnerConeAngle{0.f};                         // spot only, rads
        float mOuterConeAngle{glm::quarter_pi<float>()};    // spot only, rads
    };

private:
    static std::uint32_t sLatestLightId;

    std::uint32_t mId{0};
    Params mParams;
    glm::vec3 mPosition{0.f};
    glm::vec3 mDirection{0.f, 0.f, -1.f};  // light forward (glTF local -Z convention) in world space

public:
    static constexpr std::uint32_t LIGHTS_STAGING_BUFFER_SIZE{1 << 16};
    static SwStagingBuffer sLightsStaging;

    SwLight();
    SwLight(Params params);

    inline Params& getParams() { return mParams; }
    inline const Params& getParams() const { return mParams; }
    inline std::uint32_t getId() const { return mId; }
    inline glm::vec3& getPosition() { return mPosition; }
    inline const glm::vec3& getPosition() const { return mPosition; }
    inline glm::vec3& getDirection() { return mDirection; }
    inline const glm::vec3& getDirection() const { return mDirection; }

    Data toData(std::uint32_t nodeTransformIndex, std::uint32_t instanceIndex) const;

    static void init();
    static void cleanup();
};