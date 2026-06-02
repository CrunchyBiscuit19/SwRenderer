#include <Scene/SwSystem.h>

class SwScene;

void SwSystem::Resizable::resize() { reInitializeOnResize(); }

SwRendererContext SwSystem::sRendererContext{};

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
