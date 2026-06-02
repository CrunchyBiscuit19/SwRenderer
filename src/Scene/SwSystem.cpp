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
    refreshBatchDependencies();
}

void SwSystem::refresh() {
    refreshPushConstants();
    refreshBatchDependencies();
}

void SwSystem::initializePushConstants() {}

void SwSystem::refreshBatchDependencies() {}

void SwSystem::refreshPushConstants() {}
