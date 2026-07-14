#pragma once

#include <Data/SwFrustum.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwBuffer.h>
#include <SDL3/SDL_events.h>

#include <functional>
#include <glm/gtx/quaternion.hpp>

enum SwMovementMode {
    FREEFLY,
    DRONE,
};

struct SwRendererContext;

class SwCamera {
public:
    struct Perspective {
    private:
        glm::mat4 mView{1.f};
        glm::mat4 mProj{1.f};  // Vulkan-style: Y-flipped, reversed-Z

    public:
        Perspective() = default;
        Perspective(glm::mat4 view, glm::mat4 proj);

        const glm::mat4& getView() const { return mView; }
        const glm::mat4& getProjVk() const { return mProj; }
        glm::mat4 getProjGL() const;
    };

    struct Data {
        Perspective mPerspective;
        glm::vec3 mWorldPos{0.f};
        SwFrustum::Data mFrustum;
    };

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

    glm::vec3 mVelocity{0.f};
    glm::vec3 mPosition{0.f};
    float mPitch{0.f};
    float mYaw{0.f};
    float mSpeed{1.f};
    bool mRelativeMode{false};
    SwMovementMode mMovementMode{FREEFLY};
    SwFrustum::Data mFrustum;
    std::array<SwAllocatedBuffer, SwSwapchain::NUM_FRAME_OVERLAP> mCameraBuffers;

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

    inline SwFrustum::Data& getFrustum() { return mFrustum; }
    SwAllocatedBuffer& getCameraBuffer();
    inline bool getRelativeMode() const { return mRelativeMode; }
    inline SwMovementMode getMovementMode() const { return mMovementMode; }
    inline glm::vec3 getPosition() const { return mPosition; }
    inline float getPitch() const { return mPitch; }
    inline float getYaw() const { return mYaw; }
    inline float getSpeed() const { return mSpeed; }

    Perspective getPerspective() const;

    void setRelativeMode(bool relativeMode);
};
