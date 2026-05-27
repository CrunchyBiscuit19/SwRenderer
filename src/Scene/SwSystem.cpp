#include <Scene/SwSystem.h>

class SwScene;

SwRendererContext SwSystem::sRendererContext{};

SwSystem::SwSystem(SwScene& scene) : mScene(scene) {}

void SwSystem::initialize() {
    initializeResources();
    initializePasses();
}
