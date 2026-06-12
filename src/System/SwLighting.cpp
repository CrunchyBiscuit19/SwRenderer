#include <System/SwLighting.h>
#include <Scene/SwScene.h>

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::initializeResources() {}  // no GPU resources yet; the scene lights buffer lives on SwScene

void SwLighting::System::initializePasses() {}  // no passes yet; lights are consumed by the geometry system
