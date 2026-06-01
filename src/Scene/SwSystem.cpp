#include <Scene/SwSystem.h>

class SwScene;

SwRendererContext SwSystem::sRendererContext{};

SwSystem::SwSystem(SwScene& scene) : mScene(scene) {}

void SwSystem::initialize() {
    initializeResources();
    refreshPushConstants();
    initializePasses();
    refreshBatchDependencies();
}

void SwSystem::refresh() {
    refreshPushConstants();
    refreshBatchDependencies();
}

void SwSystem::refreshBatchDependencies() {}

void SwSystem::refreshPushConstants() {}
