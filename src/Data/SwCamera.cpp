#include <Data/SwCamera.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwRendererContext.h>
#include <Renderer/SwSwapchain.h>
#include <Scene/SwScene.h>
#include <System/SwInput.h>
#include <imgui.h>

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
}

void SwCamera::initialize() {
    for (std::uint32_t i = 0; i < SwSwapchain::NUM_FRAME_OVERLAP; i++) {
        mCameraBuffers[i] = SwBufferFactory::createAllocatedBuffer(
            "CameraBuffer" + std::to_string(i),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            sizeof(Data),
            true
        );
    }
}

SwAllocatedBuffer& SwCamera::getCameraBuffer() {
    return mCameraBuffers[SwRenderer::sRendererContext.mSwapchain->getFrameNumber() % SwSwapchain::NUM_FRAME_OVERLAP];
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
    SwInput::System& input = SwRenderer::sRendererContext.mScene->getInputSystem();

    if (input.wasTriggered(SwInput::CAMERA_TOGGLE_MODE)) mMovementMode = mMovementMode == FREEFLY ? DRONE : FREEFLY;
    if (input.wasTriggered(SwInput::CAMERA_LOOK)) mRelativeMode = !mRelativeMode;

    if (mRelativeMode) {
        const glm::vec2 lookDelta = input.getMouseDelta();
        mYaw += lookDelta.x / input.lookSensitivity();
        mPitch -= lookDelta.y / input.lookSensitivity();
    }
    mSpeed = std::clamp(mSpeed + input.getWheelDelta() * input.scrollSpeed(), 0.f, MAX_CAMERA_SPEED);

    mVelocity = glm::vec3(0.f);
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        if (mMovementMode == FREEFLY && input.isActive(SwInput::CAMERA_UP))
            mVelocity.y = 1;
        else if (mMovementMode == FREEFLY && input.isActive(SwInput::CAMERA_DOWN))
            mVelocity.y = -1;
        else {
            if (input.isActive(SwInput::CAMERA_FORWARD)) mVelocity.z = -1;
            if (input.isActive(SwInput::CAMERA_BACK)) mVelocity.z = 1;
        }
        if (input.isActive(SwInput::CAMERA_LEFT)) mVelocity.x = -1;
        if (input.isActive(SwInput::CAMERA_RIGHT)) mVelocity.x = 1;
        mVelocity *= input.moveSpeed();
    }

    SDL_SetWindowRelativeMouseMode(SwRenderer::sRendererContext.mSwapchain->getWindowPtr(), mRelativeMode);

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

    mFrustum = SwFrustum::calculateFrustum(mPosition, forward, right, up, halfHSide, halfVSide, NEAR_PLANE, FAR_PLANE);

    Data cameraData{
        .mPerspective = getPerspective(),
        .mWorldPos = mPosition,
        .mFrustum = mFrustum,
    };
    getCameraBuffer().copyFromUnchecked(&cameraData, sizeof(Data));
}

SwPerspective SwCamera::getPerspective() const {
    glm::mat4 view = getViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(FOVY), SwRenderer::sRendererContext.mSwapchain->getAspectRatio(), FAR_PLANE, NEAR_PLANE);
    proj[1][1] *= -1;
    return {view, proj};
}

void SwCamera::setRelativeMode(bool relativeMode) { mRelativeMode = relativeMode; }