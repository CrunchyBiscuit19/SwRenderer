#include <Renderer/SwRenderer.h>
#include <Scene/SwSystem.h>

class SwScene;

void SwSystem::Resizable::resize() { reInitializeOnResize(); }

SwSystem::SwSystem(SwScene& scene) : mScene(scene) {}

void SwSystem::initialize() {
    initializeResources();
    refreshPushConstants();
    initializePasses();
    refreshDependencies();
}

void SwSystem::refresh() {
    refreshPushConstants();
    refreshDependencies();
}

void SwSystem::refreshDependencies() {}

void SwSystem::refreshPushConstants() {}
