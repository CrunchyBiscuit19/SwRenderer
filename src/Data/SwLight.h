#pragma once

#include <Resource/SwBuffer.h>
#include <Data/SwInstance.h>

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
        glm::vec3 mBasePosition{0.f};
        glm::vec3 mBaseDirection{0.f, 0.f, -1.f};  // light forward (glTF local -Z convention) in node-local space
    };

    struct Params {
        Type mType{Type::Point};
        glm::vec3 mColor{1.f};
        float mIntensity{1.f};
        float mRange{5.f};
        float mInnerConeAngle{0.f};
        float mOuterConeAngle{glm::quarter_pi<float>()};
    };

private:
    static std::uint32_t sLatestLightId;

    std::uint32_t mId{0};
    Params mParams;

    // Identity of the light within the scene. Set once when the instance is spawned.
    std::uint32_t mAssetId{0};
    std::uint32_t mInstanceId{0};
    std::uint32_t mRelativeNodeIndex{0};

    // Scene-wide indices resolved from the asset offsets, refreshed whenever offsets realign.
    std::uint32_t mNodeTransformIndex{0};  // into mSceneNodeTransformsBuffer
    std::uint32_t mInstanceIndex{0};       // into mSceneInstancesBuffer

    // The light's own offset/orientation in node-local space. The node transform and instance transform
    // place it in the world as instance * nodeTransform * base.
    glm::vec3 mBasePosition{0.f};
    glm::vec3 mBaseDirection{0.f, 0.f, -1.f};

public:
    static constexpr std::uint32_t LIGHTS_STAGING_BUFFER_SIZE{1 << 16};
    static SwStagingBuffer sLightsStaging;

    SwLight();
    SwLight(Params params);

    inline Params& getParams() { return mParams; }
    inline const Params& getParams() const { return mParams; }
    inline std::uint32_t getId() const { return mId; }

    inline std::uint32_t getAssetId() const { return mAssetId; }
    inline std::uint32_t getInstanceId() const { return mInstanceId; }
    inline std::uint32_t getRelativeNodeIndex() const { return mRelativeNodeIndex; }

    inline void setInstanceContext(std::uint32_t assetId, std::uint32_t instanceId, std::uint32_t relativeNodeIndex) {
        mAssetId = assetId;
        mInstanceId = instanceId;
        mRelativeNodeIndex = relativeNodeIndex;
    }

    inline void setNodeTransformIndex(std::uint32_t index) { mNodeTransformIndex = index; }
    inline void setInstanceIndex(std::uint32_t index) { mInstanceIndex = index; }

    inline glm::vec3& getBasePosition() { return mBasePosition; }
    inline const glm::vec3& getBasePosition() const { return mBasePosition; }
    inline glm::vec3& getBaseDirection() { return mBaseDirection; }
    inline const glm::vec3& getBaseDirection() const { return mBaseDirection; }

    // World-space resolution used CPU-side for light selection and shadow matrices. Mirrors the GPU math.
    glm::vec3 worldPosition(const glm::mat4& instanceTransform, const glm::mat4& nodeWorldTransform) const;
    glm::vec3 worldDirection(const glm::mat4& instanceTransform, const glm::mat4& nodeWorldTransform) const;

    Data toData() const;

    static void init();
    static void cleanup();
};
