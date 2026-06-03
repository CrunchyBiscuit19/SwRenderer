#include <Data/SwCamera.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>

SwPerspective::SwPerspective(glm::mat4 view, glm::mat4 proj) : mView(std::move(view)), mProj(std::move(proj)) {}

glm::mat4 SwPerspective::getProjGL() const {
    glm::mat4 p = mProj;
    p[1][1] *= -1;
    return p;
}

SwCamera::SwCamera() {
    mVelocity = glm::vec3(0.f);
    mPosition = glm::vec3(0, 0, 5);
    mPitch = 0;
    mYaw = 0;
    mMovementMode = FREEFLY;

    mMovementFunctions[FREEFLY] = [this]() -> void {
        const SDL_Keymod modState = SDL_GetModState();
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_W]) {
            if (modState & KMOD_LSHIFT)
                mVelocity.y = 1;
            else
                mVelocity.z = -1;
        }
        if (keyState[SDL_SCANCODE_S]) {
            if (modState & KMOD_LSHIFT)
                mVelocity.y = -1;
            else
                mVelocity.z = 1;
        }
        if (keyState[SDL_SCANCODE_A]) mVelocity.x = -1;
        if (keyState[SDL_SCANCODE_D]) mVelocity.x = 1;
        mVelocity *= 0.1f;
    };

    mMovementFunctions[DRONE] = [this]() -> void {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_W]) mVelocity.z = -1;
        if (keyState[SDL_SCANCODE_S]) mVelocity.z = 1;
        if (keyState[SDL_SCANCODE_A]) mVelocity.x = -1;
        if (keyState[SDL_SCANCODE_D]) mVelocity.x = 1;
        mVelocity *= 0.1f;
    };
}

void SwCamera::initialize() {
    SwRenderer::sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        mVelocity = glm::vec3(0.f);
        mMovementFunctions[mMovementMode]();
        if (keyState[SDL_SCANCODE_C] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            switch (mMovementMode) {
                case FREEFLY:
                    mMovementMode = DRONE;
                    break;
                case DRONE:
                    mMovementMode = FREEFLY;
                    break;
            }
        }
        if (e.button.button == SDL_BUTTON_RIGHT && e.type == SDL_MOUSEBUTTONDOWN) mRelativeMode = static_cast<SDL_bool>(!mRelativeMode);
        if (e.type == SDL_MOUSEMOTION && mRelativeMode) {
            mYaw += static_cast<float>(e.motion.xrel) / 200.f;
            mPitch -= static_cast<float>(e.motion.yrel) / 200.f;
        }
        if (e.type == SDL_MOUSEWHEEL) {
            mSpeed += static_cast<float>(e.wheel.y) * 0.2f;
            mSpeed = std::clamp(mSpeed, 0.f, MAX_CAMERA_SPEED);
        }
    });

    const vk::DeviceSize frustumBufferSize = sizeof(SwCull::Plane) * NUM_FRUSTUM_PLANES;
    mFrustumBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, frustumBufferSize, true
    );
}

glm::mat4 SwCamera::getViewMatrix() const {
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), mPosition);
    const glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::quat SwCamera::getPitchMatrix() const { return glm::angleAxis(mPitch, glm::vec3{1.f, 0.f, 0.f}); }

glm::quat SwCamera::getYawMatrix() const {
    return glm::angleAxis(mYaw, glm::vec3{0.f, -1.f, 0.f});  // Negative Y to flip OpenGL rotation
}

glm::mat4 SwCamera::getRotationMatrix() const {
    const glm::quat pitchRotation = getPitchMatrix();
    const glm::quat yawRotation = getYawMatrix();
    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::vec3 SwCamera::getDirectionVector() const {
    glm::mat4 rot = getRotationMatrix();
    glm::vec3 forward = glm::normalize(glm::vec3(rot * glm::vec4(0, 0, -1, 0)));
    return forward;
}

glm::mat4 SwCamera::getSpawnTransform(float distance, float scale, bool rotated) const {
    glm::vec3 position = mPosition + getDirectionVector() * distance;
    glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
    if (rotated) transform *= getRotationMatrix();
    transform = glm::scale(transform, glm::vec3(scale));
    return transform;
}

void SwCamera::update(float deltaTime, float expectedDeltaTime) {
    SDL_SetRelativeMouseMode(mRelativeMode);

    switch (mMovementMode) {
        case FREEFLY:
            mPosition += glm::vec3(getYawMatrix() * glm::vec4(mVelocity * mSpeed * (deltaTime / expectedDeltaTime), 0.f));
            break;
        case DRONE:
            mPosition += glm::vec3(getRotationMatrix() * glm::vec4(mVelocity * mSpeed * (deltaTime / expectedDeltaTime), 0.f));
            break;
    }

    glm::mat4 rot = getRotationMatrix();
    glm::vec3 forward = glm::normalize(glm::vec3(rot * glm::vec4(0, 0, -1, 0)));
    glm::vec3 right = glm::normalize(glm::vec3(rot * glm::vec4(1, 0, 0, 0)));
    glm::vec3 up = glm::normalize(glm::vec3(rot * glm::vec4(0, 1, 0, 0)));

    const float halfVSide = std::tanf(glm::radians(FOVY) * .5f);
    const float halfHSide = halfVSide * SwRenderer::sRendererContext.mSwapchain->getAspectRatio();

    mFrustumPlanes[FRUSTUM_NEAR_FACE] = SwCull::Plane(forward, mPosition + forward * NEAR_PLANE);
    mFrustumPlanes[FRUSTUM_FAR_FACE] = SwCull::Plane(-forward, mPosition + forward * FAR_PLANE);
    mFrustumPlanes[FRUSTUM_LEFT_FACE] = SwCull::Plane(glm::cross(forward - right * halfHSide, up), mPosition);
    mFrustumPlanes[FRUSTUM_RIGHT_FACE] = SwCull::Plane(glm::cross(up, forward + right * halfHSide), mPosition);
    mFrustumPlanes[FRUSTUM_TOP_FACE] = SwCull::Plane(glm::cross(forward + up * halfVSide, right), mPosition);
    mFrustumPlanes[FRUSTUM_BOTTOM_FACE] = SwCull::Plane(glm::cross(right, forward - up * halfVSide), mPosition);
    // Cross product between slanted vectors and up / right vectors gives plane normals pointing inward.
    // Planes stretch indefinitely. Left, right, top, bottom planes all pass through camera position. Near and far calculate with normal * distance.
}

SwPerspective SwCamera::getPerspective() const {
    glm::mat4 view = getViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(FOVY), SwRenderer::sRendererContext.mSwapchain->getAspectRatio(), FAR_PLANE, NEAR_PLANE);
    proj[1][1] *= -1;
    return {view, proj};
}

void SwCamera::setRelativeMode(SDL_bool relativeMode) { mRelativeMode = relativeMode; }