#pragma once

#include <Data/SwLight.h>
#include <System/SwCull.h>
#include <Renderer/SwRendererContext.h>
#include <Resource/SwBuffer.h>
#include <SDL3/SDL_events.h>

#include <functional>
#include <glm/gtx/quaternion.hpp>

enum SwMovementMode {
    FREEFLY,
    DRONE,
};

struct SwPerspective {
private:
    glm::mat4 mView;
    glm::mat4 mProj;  // Vulkan-style: Y-flipped, reversed-Z

public:
    SwPerspective() = default;
    SwPerspective(glm::mat4 view, glm::mat4 proj);

    const glm::mat4& getView() const { return mView; }
    const glm::mat4& getProjVk() const { return mProj; }
    glm::mat4 getProjGL() const;
};

struct SwRendererContext;

class SwCamera {
public:
    static constexpr std::uint32_t NUM_FRUSTUM_PLANES{6};

private:
    static constexpr float FOVY{70.f};
    static constexpr float NEAR_PLANE{.1f};
    static constexpr float FAR_PLANE{10000.f};
    static constexpr std::uint32_t FRUSTUM_NEAR_FACE{0};
    static constexpr std::uint32_t FRUSTUM_FAR_FACE{1};
    static constexpr std::uint32_t FRUSTUM_LEFT_FACE{2};
    static constexpr std::uint32_t FRUSTUM_RIGHT_FACE{3};
    static constexpr std::uint32_t FRUSTUM_TOP_FACE{4};
    static constexpr std::uint32_t FRUSTUM_BOTTOM_FACE{5};

    glm::vec3 mVelocity;
    glm::vec3 mPosition;
    float mPitch{0.f};
    float mYaw{0.f};
    float mSpeed{1.f};
    bool mRelativeMode{false};
    SwMovementMode mMovementMode;
    std::unordered_map<SwMovementMode, std::function<void()>> mMovementFunctions;
    SwAllocatedBuffer mFrustumBuffer;
    std::array<SwCull::Plane, NUM_FRUSTUM_PLANES> mFrustumPlanes;

public:
    static constexpr float MAX_CAMERA_SPEED{10.f};

    SwCamera();

    static void init();

    void initialize();

    glm::mat4 getViewMatrix() const;
    glm::quat getPitchMatrix() const;
    glm::quat getYawMatrix() const;
    glm::mat4 getRotationMatrix() const;
    glm::vec3 getDirectionVector() const;
    glm::mat4 getSpawnTransform(float distance = 5.f, float scale = 1.f, bool rotated = false) const;
    void update(float deltaTime, float expectedDeltaTime);

    inline std::array<SwCull::Plane, NUM_FRUSTUM_PLANES>& getFrustumPlanes() { return mFrustumPlanes; }
    inline SwAllocatedBuffer& getFrustumBuffer() { return mFrustumBuffer; }
    inline bool getRelativeMode() const { return mRelativeMode; }
    inline SwMovementMode getMovementMode() const { return mMovementMode; }
    inline glm::vec3 getPosition() const { return mPosition; }
    inline float getPitch() const { return mPitch; }
    inline float getYaw() const { return mYaw; }
    inline float getSpeed() const { return mSpeed; }

    SwPerspective getPerspective() const;

    void setRelativeMode(bool relativeMode);
};
