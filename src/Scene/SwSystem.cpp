#include <Renderer/SwRenderer.h>
#include <Scene/SwSystem.h>

class SwScene;

void SwSystem::Resizable::resize() { reInitializeOnResize(); }

SwSystem::SwSystem(SwScene& scene) : mScene(scene) {}

void SwSystem::initialize() {
    initializeResources();
    initializePasses();
    refreshDataUsage();
}

void SwSystem::refresh() { refreshDataUsage(); }

void SwSystem::refreshDataUsage() {}
