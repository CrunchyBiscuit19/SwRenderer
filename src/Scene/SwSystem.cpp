#include <Scene/SwSystem.h>
#include <Renderer/SwRenderer.h>

class SwScene;

void SwSystem::Resizable::resize() { reInitializeOnResize(); }


SwSystem::SwSystem(SwScene& scene) : mScene(scene) {}

void SwSystem::initialize() {
    initializeResources();
    initializePushConstants();
    refreshPushConstants();
    initializePasses();
    refreshDynamicDependencies();
}

void SwSystem::refresh() {
    refreshPushConstants();
    refreshDynamicDependencies();
}

void SwSystem::initializePushConstants() {}

void SwSystem::refreshDynamicDependencies() {}

void SwSystem::refreshPushConstants() {}
