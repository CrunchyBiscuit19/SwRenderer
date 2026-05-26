#pragma once

#include <Scene/SwCull.h>
#include <Renderer/SwRendererContext.h>
#include <Resource/SwBuffer.h>
#include <SDL_events.h>

#include <functional>
#include <glm/gtx/quaternion.hpp>

enum SwMovementMode {
    FREEFLY,
    DRONE,
};

struct SwPerspective {
    glm::mat4 mView;
    glm::mat4 mProj;
};

struct SwRendererContext;

class SwCamera {
private:
    static constexpr float FOVY{70.f};
    static constexpr float NEAR_PLANE{.1f};
    static constexpr float FAR_PLANE{10000.f};
    static constexpr std::uint32_t NUM_FRUSTUM_PLANES{6};
    static constexpr std::uint32_t FRUSTUM_NEAR_FACE{0};
    static constexpr std::uint32_t FRUSTUM_FAR_FACE{1};
    static constexpr std::uint32_t FRUSTUM_LEFT_FACE{2};
    static constexpr std::uint32_t FRUSTUM_RIGHT_FACE{3};
    static constexpr std::uint32_t FRUSTUM_TOP_FACE{4};
    static constexpr std::uint32_t FRUSTUM_BOTTOM_FACE{5};

    static SwRendererContext sRendererContext;
    glm::vec3 mVelocity;
    glm::vec3 mPosition;
    float mPitch{0.f};
    float mYaw{0.f};
    float mSpeed{1.f};
    SDL_bool mRelativeMode{SDL_FALSE};
    SwMovementMode mMovementMode;
    std::unordered_map<SwMovementMode, std::function<void()>> mMovementFunctions;
    SwAllocatedBuffer mFrustumBuffer;
    std::array<SwCull::Plane, NUM_FRUSTUM_PLANES> mFrustumPlanes;

public:
    static constexpr float MAX_CAMERA_SPEED{10.f};

    SwCamera();

    static void init(SwRendererContext cameraContext);

    void initialize();

    glm::mat4 getViewMatrix() const;
    glm::quat getPitchMatrix() const;
    glm::quat getYawMatrix() const;
    glm::mat4 getRotationMatrix() const;
    glm::vec3 getDirectionVector() const;
    void update(float deltaTime, float expectedDeltaTime);

    inline std::array<SwCull::Plane, NUM_FRUSTUM_PLANES>& getFrustumPlanes() { return mFrustumPlanes; }
    inline SwAllocatedBuffer& getFrustumBuffer() { return mFrustumBuffer; }
    inline SDL_bool getRelativeMode() const { return mRelativeMode; }   
    inline SwMovementMode getMovementMode() const { return mMovementMode; }
    inline glm::vec3 getPosition() const { return mPosition; }
    inline float getPitch() const { return mPitch; }
    inline float getYaw() const { return mYaw; }
    inline float getSpeed() const { return mSpeed; }

    SwPerspective getPerspective() const;

    void setRelativeMode(SDL_bool relativeMode);
};
